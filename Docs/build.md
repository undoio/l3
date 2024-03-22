# L3 Logging Library - Build Instructions

The L3 library is supported on Linux and has been tested on the
Ubuntu 22.04.4 LTS (jammy) distro. Build steps are implemented via
this [Makefile](../Makefile), tested and supported for the
following versions of the compilers:

- gcc (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0
- g++ (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0

The L3 package consists of core L3-source files and sample
programs in the [test-use-cases/](../test-use-cases/) directory.

`Makefile` rules implement building and running tests for the
L3 package with a small collection of `.c`, and C++, `.cpp` or
`.cc`, sample programs.

## Quick-start

Check `make` help / usage: `$ make help`

## Dependencies

The L3 package depends on the [LineOfCode](https://github.com/Soft-Where-Inc/LineOfCode)
package, as a submodule.

Run the following commands before doing the L3 build.

```
$ git submodule init
$ git submodule update
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

To integrate with the Line-of-Code (LOC) package, enable the `L3_LOC_ENABLED=1`
environment variable.  Example `make` commands to build the library
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


## CI-Support

The [build.yml](../.github/workflows/build.yml) exercises these
commands in our CI system.
