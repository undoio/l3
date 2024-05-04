# SimpleMakefile

Template `Makefile` for small- and medium-sized C/C++ projects.

This `Makefile` assumes that all source files (.c or .cpp) are in the same
directory, will compile each one of them separately and then link the
resulting objects together in the executable.

### Usage

1. Put `Makefile` in the directory of your C/C++ project.
2. Edit the first lines (compiler to use, name of the target, list of source files).
3. Hit `make` in the terminal.

### Acknowledgements

The [SimpleMakefile](https://github.com/lostella/SimpleMakefile) repo has
been cloned into this as a starting project source-base extended for use with
the L3/LOC packages.

The efforts of the owner of the SimpleMakefile repo, Lorenzo Stella, are
gratefully appreciated!
