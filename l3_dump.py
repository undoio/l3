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

# ##############################################################################
# Constants that tie the unpacking logic to L3's core structure's layout
# ##############################################################################
L3_LOG_HEADER_SZ = 32  # bytes; offsetof(L3_LOG, slots)
L3_ENTRY_SZ = 32        # bytes; sizeof(struct l3_entry)

if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} <logfile> <program-binary>")
    sys.exit(0)

# pylint: disable-next=line-too-long
with subprocess.Popen(["readelf", "-x", ".rodata", sys.argv[2]], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) as section:
    stdout, stderr = section.communicate()
    rodata_offs = int(stdout.split('\n')[2].split()[0], 0)

# pylint: disable-next=line-too-long
with subprocess.Popen(["readelf", "-p", ".rodata", sys.argv[2]], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) as p:
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

    # pylint: disable-next=invalid-name
    nentries = 0
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
        if loc == 0:
            print(f"{tid=} '{strings[offs]}' {arg1=} {arg2=}")
        else:
            print(f"{tid=} {loc=} '{strings[offs]}' {arg1=} {arg2=}")

        nentries += 1

print(f"Unpacked {nentries=} Log-entries.")
