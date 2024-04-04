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

# ##############################################################################
# Constants that tie the unpacking logic to L3's core structure's layout
# ##############################################################################
L3_LOG_HEADER_SZ = 32  # bytes; offsetof(L3_LOG, slots)
L3_ENTRY_SZ = 32       # bytes; sizeof(L3_ENTRY)

if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} <logfile> <program-binary>")
    print(" ")
    print("Note: If L3 logging is done for <program-binary> with L3_LOC_ENABLED=1")
    print("we expect to find the LOC-decoder binary named <program-binary>_loc")
    sys.exit(0)

# #############################################################################
# Check for required LOC-decoder binary, based on environment of run
# If L3_LOC_ENABLED=1 is set in the env, search for a LOC-decoder binary
# named "<program_binary>_loc".
# #############################################################################
PROGRAM_BIN = sys.argv[2]
DECODE_LOC_ID = 0   # By default, we expect L3 logging was done w/LOC OFF.
LOC_ENABLED = "L3_LOC_ENABLED"
LOC_DECODER = "LOC_DECODER"
if LOC_ENABLED in os.environ:
    env_var_val = os.getenv(LOC_ENABLED)
    if env_var_val == "1":
        LOC_DECODER = PROGRAM_BIN + "_loc"
        if os.path.exists(LOC_DECODER) is False:
            print(f"Env-var {LOC_ENABLED}=1 is set, but required "
                  + f"LOC-decoder binary {LOC_DECODER} is not found.")
            sys.exit(1)

        # LOC is in-use and LOC-decoder binary was found.
        DECODE_LOC_ID = 1

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

# #############################################################################
# main() begins here ...
# #############################################################################
# pylint: disable-next=line-too-long
with subprocess.Popen(["readelf", "-x", ".rodata", PROGRAM_BIN],
                      stdout=subprocess.PIPE,
                      stderr=subprocess.PIPE, text=True) as section:
    stdout, stderr = section.communicate()
    rodata_offs = int(stdout.split('\n')[2].split()[0], 0)

# pylint: disable-next=line-too-long
with subprocess.Popen(["readelf", "-p", ".rodata", PROGRAM_BIN],
                      stdout=subprocess.PIPE,
                      stderr=subprocess.PIPE, text=True) as p:
    stdout, stderr = p.communicate()

strings = {}
for line in stdout.split('\n')[2:]:
    if line:
        offs = int(line.split()[1][:-1], 16)
        s = line.split("]  ")[1]
        strings[offs] = s

with open(sys.argv[1], 'rb') as file:
    # Unpack the 1st n-bytes as an L3_LOG{} struct to get a hold
    # of the fbase-address stashed by the l3_init() call.
    data = file.read(L3_LOG_HEADER_SZ)
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
        elif DECODE_LOC_ID == 0:
            print(f"{tid=} {loc=} '{strings[offs]}' {arg1=} {arg2=}")
        elif DECODE_LOC_ID == 1:
            # ----------------------------------------------------------------
            # Minor optimization to speed-up unpacking of L3 log-dumps from
            # tests that log millions of log-entries from the same line-of-code.
            # If we found a new location-ID, unpack it.
            if loc != loc_prev:
                UNPACK_LOC = exec_binary([LOC_DECODER, '--brief', str(loc)])

                # Save-off location-ID and unpacked string for next loop
                unpack_loc_prev = UNPACK_LOC
                loc_prev = loc
            else:
                UNPACK_LOC = unpack_loc_prev
            print(f"{tid=} {UNPACK_LOC} '{strings[offs]}' {arg1=} {arg2=}")

        nentries += 1

print(f"Unpacked {nentries=} log-entries.")
