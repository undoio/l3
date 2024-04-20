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

# ##############################################################################
# Constants that tie the unpacking logic to L3's core structure's layout
# ##############################################################################
L3_LOG_HEADER_SZ = 32  # bytes; offsetof(L3_LOG, slots)
L3_ENTRY_SZ = 32       # bytes; sizeof(L3_ENTRY)

# #############################################################################
# #############################################################################
PROGRAM_BIN = 'unknown'
DECODE_LOC_ID = 0   # By default, we expect L3 logging was done w/LOC OFF.
LOC_ENABLED = "L3_LOC_ENABLED"
LOC_DECODER = "LOC_DECODER"

# #############################################################################
# Establish path / names to tools used here based on the OS-platform version
# #############################################################################
OS_UNAME_S = platform.system()

if OS_UNAME_S == 'Linux':
    READELF_BIN = 'readelf'
    READELF_HEXDUMP_ARG = '-x'
    READELF_STRDUMP_ARG = '-p'
    READELF_STRING_SEP = ']  '
    READELF_DATA_SECTION = '.rodata'

# #############################################################################
def check_usage():
    """Simple usage check."""
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <logfile> <program-binary>")
        print(" ")
        print("Note: If L3 logging is done for <program-binary> with L3_LOC_ENABLED=1")
        print("we expect to find the LOC-decoder binary named <program-binary>_loc")
        sys.exit(0)

# #############################################################################
def set_decode_loc_id():
    """
    Check for required LOC-decoder binary, based on environment of run
    If L3_LOC_ENABLED=1 is set in the env, search for a LOC-decoder binary
    named "<program_binary>_loc".
    """
    program_bin = sys.argv[2]
    decode_loc_id = DECODE_LOC_ID
    loc_decoder = LOC_DECODER
    if LOC_ENABLED in os.environ:
        env_var_val = os.getenv(LOC_ENABLED)
        if env_var_val == "1":
            loc_decoder = program_bin + "_loc"
            if os.path.exists(loc_decoder) is False:
                print(f"Env-var {LOC_ENABLED}=1 is set, but required "
                      + f"LOC-decoder binary {loc_decoder} is not found.")
                sys.exit(1)

            # LOC is in-use and LOC-decoder binary was found.
            decode_loc_id = 1

    return (program_bin, decode_loc_id, loc_decoder)

# #############################################################################
def which_binary(os_uname_s:str, bin_name:str):
    """
    Check to see if required binary is found in $PATH. Error out otherwise.
    """
    if shutil.which(bin_name) is None:
        print(f"Required {os_uname_s} binary '{bin_name}' is not found in $PATH.")
        sys.exit(1)

# #############################################################################
def parse_rodata_start_addr(program_bin:str) -> int:
    """
    Parse output from `readelf` to extract start address of section '.rodata'

    We are parsing the following sample hexdump output on Linux:

Hex dump of section '.rodata':
  0x00002000 01000200 00000000 2f746d70 2f6c332e ......../tmp/l3.
  ^^^^^^^^^^

    Return int for 1st hex-value; i.e., 0x00002000 .
    """
    with subprocess.Popen([READELF_BIN, READELF_HEXDUMP_ARG,
                           READELF_DATA_SECTION, program_bin],
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True) as section:
        _stdout, _stderr = section.communicate()

        _rodata_offs = parse_rodata_start_offset(_stdout)
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
    with subprocess.Popen([READELF_BIN, READELF_STRDUMP_ARG,
                           READELF_DATA_SECTION,
                           program_bin],
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True) as rodata_hexdump:
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

    # Return # of valid-lines-parsed count, so pytests can verify it.
    return (_strings, nlines)

# #############################################################################
def exec_binary(cmdargs:list):
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

###############################################################################
# main() driver
###############################################################################
# pylint: disable-msg=too-many-locals
def main():
    """
    Shell to call do_main() with command-line arguments.
    """
    check_usage()
    (program_bin, decode_loc_id, loc_decoder_bin) = set_decode_loc_id()

    # Validate that required binary used below are found in $PATH.
    which_binary(OS_UNAME_S, READELF_BIN)

    rodata_offs = parse_rodata_start_addr(program_bin)

    strings = parse_rodata_string_offsets(program_bin)

    with open(sys.argv[1], 'rb') as file:
        # Unpack the 1st n-bytes as an L3_LOG{} struct to get a hold
        # of the fbase-address stashed by the l3_init() call.
        data = file.read(L3_LOG_HEADER_SZ)

        # pylint: disable-next=unused-variable
        idx, loc, fibase, _, _ = struct.unpack('<iiQQQ', data)
        # print(f"{idx=} {fibase=:x}")

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

            tid, loc, ptr, arg1, arg2 = struct.unpack('<iiQQQ', row)

            # If no entry was logged, ptr to message's string is expected to be NULL
            if ptr == 0:
                break

            offs = ptr - fibase - rodata_offs

            # No location-ID will be recorded in log-files if L3_LOC_ENABLED is OFF.
            if loc == 0:
                print(f"{tid=} '{strings[offs]}' {arg1=} {arg2=}")
            elif decode_loc_id == 0:
                print(f"{tid=} {loc=} '{strings[offs]}' {arg1=} {arg2=}")
            elif decode_loc_id == 1:
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
                print(f"{tid=} {UNPACK_LOC} '{strings[offs]}' {arg1=} {arg2=}")

            nentries += 1

    print(f"Unpacked {nentries=} log-entries.")

###############################################################################
# Start of the script: Execute only if run as a script
###############################################################################
if __name__ == "__main__":
    main()
