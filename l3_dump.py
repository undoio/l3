#!/usr/bin/env python
import sys
import struct
import subprocess

if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} logfile exe")
    sys.exit(1)

section = subprocess.Popen(["readelf", "-x", ".rodata", sys.argv[2]], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
stdout, stderr = section.communicate()
rodata_offs = int(stdout.split('\n')[2].split()[0], 0)

p = subprocess.Popen(["readelf", "-p", ".rodata", sys.argv[2]], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
stdout, stderr = p.communicate()
strings = {}
for line in stdout.split('\n')[2:]:
    if line:
        offs = int(line.split()[1][:-1], 16)
        s = line.split("]  ")[1]
        strings[offs] = s

with open(sys.argv[1], 'rb') as file:
    data = file.read(32)
    idx, fibase, _, _ = struct.unpack('<QQQQ', data)
    print(f"{idx=} {fibase=:x}")

    for _ in range(3):
        row = file.read(32)
        tid, ptr, arg1, arg2 = struct.unpack('<QQQQ', row)
        offs = ptr-fibase - rodata_offs
        print(f"{tid=} '{strings[offs]}' {arg1=} {arg2=}")


