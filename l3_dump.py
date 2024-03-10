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
    data = file.read(32)
    idx, loc, fibase, _, _ = struct.unpack('<iiQQQ', data)
    # print(f"{idx=} {fibase=:x}")

    for _ in range(3):
        row = file.read(32)
        tid, loc, ptr, arg1, arg2 = struct.unpack('<iiQQQ', row)
        offs = ptr - fibase - rodata_offs
        if loc == 0:
            print(f"{tid=} '{strings[offs]}' {arg1=} {arg2=}")
        else:
            print(f"{tid=} {loc=} '{strings[offs]}' {arg1=} {arg2=}")
