#!/usr/bin/env python3
"""
Python script to unpack L3 logging file into human-readable trace messages.
L3: Lightweight Logging Library, Version 0.1
Date 2023-12-24
Copyright (c) 2023-2024
"""
import sys
import struct
import subprocess
import os
import platform
import shutil
import re
import shlex
import argparse

# ##############################################################################
# Constants that tie the unpacking logic to L3's core structure's layout
# ##############################################################################
L3_LOG_HEADER_SZ = 32  # bytes; offsetof(L3_LOG, slots)
L3_ENTRY_SZ = 32       # bytes; sizeof(L3_ENTRY)

# #############################################################################
PROGRAM_BIN = 'unknown'
DECODE_LOC_ID = 0   # By default, we expect L3 logging was done w/LOC OFF.
LOC_ENABLED = "L3_LOC_ENABLED"
LOC_DECODER = "LOC_DECODER"

# -----------------------------------------------------------------------------
# The driving env-var, L3_LOC_ENABLED, needs to be set to one of these values.
# LOC-encoding comes in two flavours. Default technique is based on the
# Python-generator script. Enhanced technique is based on LOC-ELF
# encoding.
# -----------------------------------------------------------------------------
L3_LOC_UNSET            = 0
L3_LOC_DEFAULT          = 1
L3_LOC_ELF_ENCODING     = 2

# #############################################################################
# Establish path / names to tools used here based on the OS-platform version
# #############################################################################
OS_UNAME_S = platform.system()

if OS_UNAME_S == 'Linux':
    READELF_BIN = 'readelf'
    READELF_HEXDUMP_ARG = '-x'
    READELF_STRDUMP_ARG = '-p'
    READELF_DATA_SECTION = '.rodata'

elif OS_UNAME_S == 'Darwin':
    READELF_BIN = 'llvm-readelf'
    READELF_HEXDUMP_ARG = '-x'
    READELF_STRDUMP_ARG = '-p'
    READELF_DATA_SECTION = '__cstring'

else:
    # To avoid pylint errors on CI-Mac/OSX builds.
    READELF_BIN = 'readelf'
    READELF_HEXDUMP_ARG = '-x'
    READELF_STRDUMP_ARG = '-p'
    READELF_DATA_SECTION = '.rodata'

# #############################################################################
# Enum types defined in src/l3.c for L3_LOG()->{platform, loc_type} fields
L3_LOG_PLATFORM_LINUX           = 1
L3_LOG_PLATFORM_MACOSX          = 2

L3_LOG_LOC_NONE                 = 0
L3_LOG_LOC_ENCODING             = 1
L3_LOG_LOC_ELF_ENCODING         = 2

# #############################################################################
def which_binary(os_uname_s:str, bin_name:str):
    """
    Check to see if required binary is found in $PATH. Error out otherwise.
    """
    which_bin = shutil.which(bin_name)
    if which_bin is None:
        print(f"Required {os_uname_s} binary '{bin_name}' is not found in $PATH.")
        sys.exit(1)

    if OS_UNAME_S == 'Darwin':
        print(f"# Using {bin_name}: {which_bin=}")

# #############################################################################
def parse_rodata_start_addr(program_bin:str) -> int:
    """
    Parse output from `readelf` to extract start address of section '.rodata'

    We are parsing the following sample hexdump output on Linux:

Hex dump of section '.rodata':
  0x00002000 01000200 00000000 2f746d70 2f6c332e ......../tmp/l3.

    On Mac/OSX, the output will be something like:

Hex dump of section '__cstring':
0x100003e94 2f746d70 2f6c332e 632d736d 616c6c2d /tmp/l3.c-small-

  ^^^^^^^^^^

    Return int for 1st hex-value; i.e., 0x00002000 .
    """
    with subprocess.Popen([READELF_BIN, READELF_HEXDUMP_ARG,
                           READELF_DATA_SECTION, program_bin],
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True) as section:
        _stdout, _stderr = section.communicate()

        # DEBUG: print(_stdout)

        _rodata_offs = parse_rodata_start_offset(_stdout)
        # DEBUG: print(f"{_rodata_offs=} {_rodata_offs=:x}")

        return _rodata_offs

# #############################################################################
def parse_rodata_start_offset(input_str:str) -> int:
    """
    Parse output from `readelf` to extract start offset of .rodata section.
    Returns: Start of `.rodata` section's hex-address as int.
    """
    _offset = -1

    # --------------------------------------------------------------------------
    # Compile a r.e. with named capture-group to extract the 1st hex-addr.
    # We expect output to be: 0x00002000 01000200 ...
    # Parse leading '0x' as optional, in case `readelf` on some platforms
    # does not spit that out.
    # --------------------------------------------------------------------------
    hexdump_pat = re.compile(r'(\s*)(0x)?(?P<offset_hex>[0-9a-fA-F]+)(.*)')
    for line in input_str.split('\n'):
        # Skip any leader lines not matching required offset-string lines.
        line_match = hexdump_pat.match(line)
        if line_match is None:
            continue

        _offset = int(line_match.group('offset_hex'), 16)
        break

    return _offset


# #############################################################################
def parse_rodata_string_offsets(program_bin:str) -> dict:
    """
    Parse output from `readelf` to extract start offset of embedded strings.
    The expected convention is that the strings are null-terminated in the
    hexdump output.

    We are parsing the following sample hexdump output on Linux:

String dump of section '.rodata':
  [     8]  /tmp/l3.c-small-unit-test.dat
  [    26]  Simple-log-msg-Args(1,2)
  [    3f]  Simple-log-msg-Args(3,4)

    Build a dictionary from 'int-offset' -> string; i.e.
        [ 8] : "/tmp/l3.c-small-unit-test.dat"
        [38] : "Simple-log-msg-Args(1,2)"

    ... and so on.
    """
    # Run: llvm-readelf -p __cstring  ./build/release/bin/unit/l3_dump.py-test
    # NOTE: For some tests, we were unable to hexdump the .rodata section and
    # were running into this error:
    #   File "/usr/lib/python3.10/subprocess.py", line 2059, in _communicate
    #       stdout = self._translate_newlines(stdout,
    #   File "/usr/lib/python3.10/subprocess.py", line 1031, in _translate_newlines
    #       data = data.decode(encoding, errors)
    #
    # pylint: disable-next=line-too-long
    #   UnicodeDecodeError: 'utf-8' codec can't decode byte 0xea in position 1371: invalid continuation byte
    #
    # Hence, the errors='ignore' clause below.

    with subprocess.Popen([READELF_BIN, READELF_STRDUMP_ARG,
                           READELF_DATA_SECTION,
                           program_bin],
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True,
                          errors='ignore') as rodata_hexdump:
        _stdout, _stderr = rodata_hexdump.communicate()

    # pylint: disable-next=unused-variable
    (_strings, nlines) = parse_string_offsets(_stdout)
    return _strings

# #############################################################################
def parse_string_offsets(input_str:str) -> (dict,int):
    """
    Parse an input string that is stdout from `readelf`. Extract start offset
    of embedded strings. Return a dictionary mapping {offset: string}.

    The expected convention is that the strings are null-terminated in the
    hexdump output. We only want to parse lines that list string offsets, like:

  [     8]  /tmp/l3.c-small-unit-test.dat
  [    26]  Simple-log-msg-Args(1,2)
  [    3f]  Simple-log-msg-Args(3,4)

    Returns: Dictionary mapping {offset: string} and # of valid-lines parsed.
    """
    # DEBUG: print(input_str)

    _strings = {}

    # --------------------------------------------------------------------------
    # Compile a r.e. with named capture-groups to extract offset / msg fields.
    # Allow for white-spaces in diff parts of the line around '[ ]' and in
    # the front, and between phrases. This is designed to cope liberally with
    # `readelf` output that may vary slightly on diff platforms; e.g. Mac/OSX.
    # --------------------------------------------------------------------------
    off_string_pat = re.compile(r'(\s*)\[\s*(?P<offset_hex>[0-9a-fA-F]+)(\s*)\](\s+)(?P<msg>.*)')
    nlines = 0
    for line in input_str.split('\n'):

        # Skip any leader lines not matching required offset-string lines.
        line_match = off_string_pat.match(line)
        if line_match is None:
            continue

        nlines += 1
        _offs = int(line_match.group('offset_hex'), 16)
        str_start = line_match.group('msg')
        # print(f"{_offs=}, {str_start=}")
        _strings[_offs] = str_start

    # DEBUG: print(_strings)

    # Return # of valid-lines-parsed count, so pytests can verify it.
    return (_strings, nlines)

# #############################################################################
def l3_unpack_loghdr(file_hdl) -> (int, int, int):
    """
    Unpack the header struct of a L3-log file and identify key fields.
    We are unpacking a struct laid out like the following:

    typedef struct l3_log
    {
        uint64_t        idx;
        uint64_t        fbase_addr;
        uint32_t        pad0;
        uint16_t        log_size;   // # of log-entries == L3_MAX_SLOTS
        uint8_t         platform;
        uint8_t         loc_type;
        uint64_t        pad1;
        L3_ENTRY        slots[L3_MAX_SLOTS];
    } L3_LOG;

    Arguments:
        file_hdl    - Handle to open log-file

    Returns: Tuple of 3-ints: (fibase, l3_platform, decode_loc_id)
    """
    data = file_hdl.read(L3_LOG_HEADER_SZ)

    # '<' => byte-order of the header is little-endian
    # See: https://docs.python.org/3/library/struct.html
    # pylint: disable-next=unused-variable
    (_, fibase, pad0, pad2, l3_platform, loc_type, pad1) = struct.unpack('<QQihBBQ', data)

    # Interpret LOC-encoding scheme flag as loc-encoding type-ID
    if loc_type == L3_LOG_LOC_ENCODING:
        decode_loc_id = L3_LOC_DEFAULT
    elif loc_type == L3_LOG_LOC_ELF_ENCODING:
        decode_loc_id = L3_LOC_ELF_ENCODING
    else:
        decode_loc_id = L3_LOC_UNSET

    # DEBUG: print(f"{l3_platform=}, {decode_loc_id=}")
    return (fibase, l3_platform, decode_loc_id)


# #############################################################################
def select_loc_decoder_bin(decode_loc_id:int, program_bin:str,
                           loc_decoder_bin:str):
    """
    Based on the type of LOC-encoding scheme found in the log-header, i.e.,
    decode_loc_id, and the input program binary's name, figure out which
    LOC-decoder binary to be used while unpacking LOC-ID values that may be
    encoded in log-entries.

    Currently, we only support decoding LOC-IDs encoded by default LOC scheme.
     - Search for a LOC-decoder binary specified by the --loc-binary argument.
     - Otherwiwse, search for a default LOC-binary named "<program_binary>_loc".
    """
    loc_decoder = None
    if decode_loc_id == L3_LOC_DEFAULT:
        loc_decoder = program_bin + "_loc" if loc_decoder_bin is None else loc_decoder_bin
        if os.path.exists(loc_decoder) is False:
            print(f"L3-log file uses default L3_LOC_ENCODING={decode_loc_id} "
                  +  " encoding scheme, but the required "
                  + f"LOC-decoder binary {loc_decoder} is not found.")
            sys.exit(1)

    return loc_decoder

# #############################################################################
def exec_binary(cmdargs:list) -> str:
    """Execute a binary, with args, and print output to stdout."""
    try:
        with subprocess.Popen(cmdargs, stdout=subprocess.PIPE, text=True) as output:
            sp_stdout, sp_stderr = output.communicate()

    except subprocess.CalledProcessError as exc:
        print(f"sp.run() Status: FAIL, rc={exc.returncode}"
              + "\nstderrr={stderr} \nargs={exc.args}"
              + "\nstdout={exc.stdout}"
              + "\nstderr={exc.stderr}")

    if sp_stderr is None:
        return sp_stdout.strip('\n')
    return sp_stdout + " " + sp_stderr

# #############################################################################
def mac_get__cstring_offset(program_bin:str) -> int:
    """
    Helper method to crack-open binary being examined on Mac/OSX and to extract
    out the 'offset' at which 'Section __cstring' starts.

    This data should really be extracted from within the program binary, like
    it's done on Liux, as part of l3_init(). But somehow the Mach-O interface
    for locating that offset is not very evident.

    For now, do this round-about thing to decode using `size` utility available
    on Mac/OSX.

     $ xcrun size -x -l -m <program_bin>

    And post-process stdout from that looking for a line like:

        Section __cstring: 0x1f3 (addr 0x100003dc4 offset 15812)

    ... to extract out the last number '15812'
    """

    assert OS_UNAME_S == 'Darwin'

    # -----------------------------------------------------------------------
    # NOTE: We really would like to do this field extraction in one cmd as:
    # pylint: disable-next=line-too-long
    #   xcrun size -x -l -m <program-bin> | grep __cstring | cut -f2 -d'(' | awk '{print $NF}' | cut -f1 -d')'
    #
    # But for some reason, splitting this long grep-cmd into args and
    # pumping it through exec_binary() is never working.
    # For now, resort to a simpler command and post-process downstream.
    # -----------------------------------------------------------------------

    grep_cmd = r'''xcrun size -x -l -m ''' + program_bin

    cmdargs = shlex.split(grep_cmd)
    _stdout = exec_binary(cmdargs)

    # -----------------------------------------------------------------------
    # Construct a r.e. to match a line in the output like:
    #   Section __cstring: 0x1f3 (addr 0x100003dc4 offset 15812)
    #
    # We want to extract '15812' out of 'offset 15812)'
    # -----------------------------------------------------------------------
    offset = -1
    cstring_re_pat = re.compile(r'(.*)__cstring:(.*)(\s+)offset(\s+)(?P<offset_int>(\d+))\)')
    for line in _stdout.split('\n'):
        # DEBUG: print(line)
        line_match = cstring_re_pat.match(line)
        if line_match is None:
            continue

        offset = int(line_match.group('offset_int'))

    # DEBUG: print(f"{offset=}")
    assert offset != -1
    return offset

###############################################################################
def do_c_print(msg_text:str, arg1:int, arg2:int) -> str:
    """
    Parse an input C-style format-string to convert it to a form that is
    accepted by Python. Invoke C-style printing using provided arguments.
    If there is any exception in printing, fall-back to just dumping the
    input arguments, without any modifications.

    Returns: The C-format msg-string replaced with arguments
    """
    format_string = fmtstr_replace(msg_text)
    # DEBUG: print(f"{format_string=}")
    msg_text = format_string % (arg1, arg2)
    return msg_text

###############################################################################
def fmtstr_replace(fmtstr:str) -> str:
    """
    Perform couple of in-place format-specifier replacement to a form that
    is supported by Python's print() method.
    For now, do two simple subsitutions, in order, of:
        - '0x%p' in format string with '0x%x'
        - '%p' in format string with '0x%x'
        - Replace all specifiers for unsigned-int with plain '%d'
    """
    format_string = fmtstr.replace('0x%p', '0x%x', 2)
    format_string = format_string.replace('%p', '0x%x', 2)
    format_string = format_string.replace('%u', '%d', 2)
    format_string = format_string.replace('%lu', '%d', 2)
    format_string = format_string.replace('%llu', '%d', 2)
    return format_string

###############################################################################
# main() driver
###############################################################################
# pylint: disable-msg=too-many-locals
def main():
    """
    Shell to call do_main() with command-line arguments.
    """
    do_main(sys.argv[1:])

###############################################################################
# pylint: disable-msg=too-many-statements
# pylint: disable-msg=too-many-branches
def do_main(args:list, return_logentry_lists:bool = False):
    """
    Main method that drives the unpacking of the L3-dump file to print
    diagnostic data. This modularized method exists outside of main() so that
    it can be called independently via pytests.

    Arguments:
        args[0] - Str: Name of L3-log file
        args[1] - Str: Name of binary that generated L3-log file.

    Returns: A collection of output things:
        - # entries processed
        - Individual lists for fields unpacked from set of log-entries processed.
            - Thread-ID, LOC-ID, print message, arg1, arg2.
    """

    # Parse command-line arguments into script locals
    parsed_args = l3_parse_args(args)

    l3_logfile  = parsed_args.log_file
    program_bin = parsed_args.prog_binary
    loc_decoder_bin = parsed_args.loc_binary

    # Validate that required binary used below are found in $PATH.
    which_binary(OS_UNAME_S, READELF_BIN)

    rodata_offs = parse_rodata_start_addr(program_bin)

    strings = parse_rodata_string_offsets(program_bin)
    if OS_UNAME_S == 'Darwin':
        cstring_off = mac_get__cstring_offset(program_bin)
        # DEBUG: print(strings)

    # To enable unit-testing, via pytests, the parsing and return-data logic
    # build lists for the data as it's being cracked open.
    tid_list = []
    loc_list = []
    msg_list = []
    arg1_list = []
    arg2_list = []

    with open(l3_logfile, 'rb') as file:
        # Unpack the 1st n-bytes as an L3_LOG{} struct to get a hold
        # of the fbase-address stashed by the l3_init() call.
        (fibase, _, decode_loc_id) = l3_unpack_loghdr(file)

        loc_decoder_bin = select_loc_decoder_bin(decode_loc_id,
                                                 program_bin,
                                                 loc_decoder_bin)
        # pylint: disable=invalid-name
        nentries = 0
        loc_prev = 0
        # Keep reading chunks of log-entries from file ...
        while True:
            row = file.read(L3_ENTRY_SZ)
            len_row = len(row)

            # Deal with eof
            if not row or len_row == 0 or len_row < L3_ENTRY_SZ:
                break

            tid, loc, msgptr, arg1, arg2 = struct.unpack('<iiQQQ', row)

            # If no entry was logged, ptr to message's string is expected to be NULL
            if msgptr == 0:
                break

            # print(f"{msgptr=}, {fibase=}, {rodata_offs=}")

            if OS_UNAME_S == 'Linux':
                offs = msgptr - fibase - rodata_offs

            elif OS_UNAME_S == 'Darwin':
                offs = msgptr - fibase - cstring_off

            else:
                offs = 0

            # print(f"{msgptr=:x}, {fibase=:x}, {rodata_offs=:x}, {offs=}")

            # Generate C-style sprintf() output on message-string.
            msg_text = do_c_print(strings[offs], arg1, arg2)

            # No location-ID will be recorded in log-files if L3_LOC_ENABLED is OFF.
            UNPACK_LOC = ''
            if loc == 0:

                # ----------------------------------------------------------------
                # We found a 0 LOC-ID but as per the L3-log header, some
                # LOC-encoding scheme was in effect. So, it's sort-off odd
                # to find a 0 LOC-ID. Report it, to tag investigation.
                if decode_loc_id != L3_LOC_UNSET:
                    print(f"{tid=} {loc=} '{msg_text}'")
                else:
                    print(f"{tid=} '{msg_text}'")

            elif decode_loc_id == L3_LOC_UNSET:

                # ----------------------------------------------------------------
                # This is a potential error somewhere, that no LOC-encoding scheme
                # was in-effect in the build, but we found a non-zero LOC-ID!
                print(f"{tid=} {loc=} '{msg_text}'")

            elif decode_loc_id == L3_LOC_DEFAULT:
                # ----------------------------------------------------------------
                # Minor optimization to speed-up unpacking of L3 log-dumps from
                # tests that log millions of log-entries from the same line-of-code.
                # If we found a new location-ID, unpack it.
                if loc != loc_prev:
                    UNPACK_LOC = exec_binary([loc_decoder_bin, '--brief', str(loc)])

                    # Save-off location-ID and unpacked string for next loop
                    unpack_loc_prev = UNPACK_LOC
                    loc_prev = loc
                else:
                    UNPACK_LOC = unpack_loc_prev

                print(f"{tid=} {UNPACK_LOC} '{msg_text}'")

            elif decode_loc_id == L3_LOC_ELF_ENCODING:
                print(f"{tid=} {loc=} '{msg_text}'")

            # Build output-lists, if requested
            if return_logentry_lists is True:
                tid_list.append(tid)

                # Strip trailing blanks, so we can match correctly against
                # expected loc_list generated in pytest, l3_dump_test.py
                loc_list.append(UNPACK_LOC.rstrip())

                msg_list.append(msg_text)
                arg1_list.append(arg1)
                arg2_list.append(arg2)

            nentries += 1

    print(f"Unpacked {nentries=} log-entries.")
    return (nentries, tid_list, loc_list, msg_list, arg1_list, arg2_list)

# #############################################################################
def l3_parse_args(args:list):
    """
    Parse command-line arguments. Return parsed-arguments object
    """
    # ---------------------------------------------------------------
    # Start of argument parser, with inline examples text
    # Create 'parser' as object of type ArgumentParser
    # ---------------------------------------------------------------
    parser  = argparse.ArgumentParser(description='Unpack L3 log-entries from log-file'
                                                 + ' generated by named program',
                                      formatter_class=argparse.RawDescriptionHelpFormatter,
                                      epilog=r'''Examples:

- Basic usage:
    ''' + sys.argv[0]
        + ''' --log-file <L3-log-file> --binary <program-binary>

NOTE: If <program-binary>, built with L3_LOC_ENABLED=1, invokes L3-logging,
      we expect to find a corresponding LOC-decoder binary named
      <program-binary>_loc, needed for decoding LOC-ID entries in the log-file.
''')

    # ======================================================================
    # Define required arguments supported by this script
    parser.add_argument('--log-file', dest='log_file'
                        , metavar='<log-file-name>'
                        , required=True
                        , help='L3 log-file name')

    parser.add_argument('--binary', dest='prog_binary'
                        , metavar='<program-binary>'
                        , required=True
                        , help='Program binary generating L3 logging')

    # ======================================================================
    # Optional arguments.
    parser.add_argument('--loc-binary', dest='loc_binary'
                        , metavar='<binary-to-decode-LOC-ID-values>'
                        , default=None
                        , help='Binary to decode LOC-ID encoded values.')

    # ======================================================================
    # Debugging support
    parser.add_argument('--verbose', dest='verbose'
                        , action='store_true'
                        , default=False
                        , help='Show verbose progress messages')

    parser.add_argument('--debug', dest='debug_script'
                        , action='store_true'
                        , default=False
                        , help='Turn on debugging for script\'s execution')

    parsed_args = parser.parse_args(args)

    if parsed_args is False:
        parser.print_help()

    return parsed_args

###############################################################################
# Start of the script: Execute only if run as a script
###############################################################################
if __name__ == "__main__":
    main()
