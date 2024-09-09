# L3 Logging Library - Build Instructions

The L3 library is supported on Linux and has been tested on
Linux Ubuntu 22.04.4 LTS (jammy) distro, and on Mac/OSX Monterey, v12.1.

Build steps are implemented via this [Makefile](../Makefile), tested and
supported for the following versions of the compilers:

- Linux:
    - gcc (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0
    - g++ (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0

- Mac:
    - gcc (Apple clang version 13.1.6)
    - g++ (Apple clang version 13.1.6)

## Prerequisites

The [l3_dump.py](../l3_dump.py) Python script uses the `readelf` binary
to decode L3 log-dump files. The `readelf` binary is usually part of the
`binutils` package. Install this on your machine and update your `$PATH`
to access `readelf`.

## Quick-start

The L3 package consists of core L3-source files and sample
programs in the [test-use-cases/](../test-use-cases/) directory.

`Makefile` rules implement building and running tests for the
L3 package with a small collection of `.c`, and C++, `.cpp` or
`.cc`, sample programs.

Check `make` help / usage: `$ make help` and closely follow the
commands specified.

You need to correctly specify the compiler to use, either
gcc or g++, in order to correctly build the different
sample programs and unit-test case sources.

## Dependencies

The L3 package depends on the [LineOfCode](https://github.com/Soft-Where-Inc/LineOfCode)
package, as a submodule.

Run the following commands before doing the L3 build.

```
$ git submodule init
$ git submodule update --remote
```

## Build usage

- We support building in `release` (default) or `debug` mode,
controlled by the `BUILD_MODE={release,debug}` environment
variable.
- All build outputs are produced in a directory named `build/` at
the top-level root-directory.

To build C-sample programs and run unit-tests:
```
$ make clean && CC=gcc LD=g++ make all-c-tests
$ make run-c-tests
```

You can also use a single target as:
```
$ make clean && CC=gcc LD=g++ make run-c-tests
```

To build C++-sample .cpp programs and run unit-tests:
```
$ make clean-l3 && CC=g++ CXX=g++ LD=g++ make all-cpp-tests
$ make run-cpp-tests
```

To build C++-sample .cc programs and run unit-tests:
```
$ make clean-l3 && CC=g++ CXX=g++ LD=g++ make all-cc-tests
$ make run-cc-tests
```

To see verbose build outputs use: `$ BUILD_VERBOSE=1 make ...`

## Integration with the LOC package

To integrate with the Line-of-Code (LOC) package, enable the `L3_LOC_ENABLED`
environment variable.  Valid values are:

1. `L3_LOC_ENABLED=1` : Default LOC-encoding, using Python generator script
2. `L3_LOC_ENABLED=2` : LOC-encoding based on ELF-named-section.

Example `make` commands to build the library
and to run the sample programs are shown below:

```
$ make clean

$ L3_LOC_ENABLED=1 CC=gcc LD=g++ make all-c-tests

$ L3_LOC_ENABLED=1 make run-c-tests
```

```
$ make clean-l3

$ L3_LOC_ENABLED=1 CC=g++ CXX=g++ LD=g++ make all-cpp-tests

$ L3_LOC_ENABLED=1 make run-cpp-tests
```

```
$ make clean-l3

$ L3_LOC_ENABLED=1 CC=g++ CXX=g++ LD=g++ make all-cc-tests

$ L3_LOC_ENABLED=1 make run-cc-tests

```

------

The standalone `l3_dump.py` Python script, invoked under the `make` targets also
recognizes the `L3_LOC_ENABLED=1` to unpack the encoded code-location ID to the
file-name / line number.

As an example, if you run this tool, by default, the LOC-ID value is reported by itself.
See, e.g., `loc=65620` in the output below.

```
$ ./l3_dump.py /tmp/l3.cc-small-test.dat ./build/release/bin/test-use-cases/single-file-CC-program
tid=10550 loc=65620 'Log-msg-Args(1,2)' arg1=1 arg2=2
tid=10550 loc=65621 'Potential memory overwrite (addr, size)' arg1=3735927486 arg2=1024
tid=10550 loc=65622 'Invalid buffer handle (addr)' arg1=3203378125 arg2=0
```

To decode the code-location, run with the `L3_LOC_ENABLED=1` environment variable, as
shown below. Under this environment-variable, the Python script requires a
LOC-generated decoder binary named `<_program-name_>_loc`, to perform the decoding.

In the example below, a decoder binary named
`./build/release/bin/test-use-cases/single-file-CC-program_loc`
is expected to be found in the `build/` area.

See the decoded `single-file-CC-program/test-main.cc:84` in the output below:

```
$ L3_LOC_ENABLED=1 ./l3_dump.py /tmp/l3.cc-small-test.dat ./build/release/bin/test-use-cases/single-file-CC-program
tid=10550 single-file-CC-program/test-main.cc:84  'Log-msg-Args(1,2)' arg1=1 arg2=2
tid=10550 single-file-CC-program/test-main.cc:85  'Potential memory overwrite (addr, size)' arg1=3735927486 arg2=1024
tid=10550 single-file-CC-program/test-main.cc:86  'Invalid buffer handle (addr)' arg1=3203378125 arg2=0
```


## CI-Support

The [build.yml](../.github/workflows/build.yml) exercises these
commands in our CI system.
