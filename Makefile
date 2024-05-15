# #############################################################################
# L3: Simple Makefile to build and test Lightweight Logging Library sources
#
# Developed based on SplinterDB (https://github.com/vmware/splinterdb) Makefile
# Modelled on the lines of the Makefile of the LineOfCode project, with which
# L3 will be integrated (See: https://github.com/Soft-Where-Inc/LineOfCode)
#
# This repo contains the core L3 library files: l3.h l3.c, l3.S (assembly)
# It contains test-use-case in C and C++. The core L3 objects have to be
# separately linked with both the C and C++ use-case sample programs.
#
# Therefore, this Makefile is written in a way to compile both the C and C++
# sources with gcc and g++ compilers, respectively. To allow linking the .S
# assembly file with C++ sources, we have to compile l3.c also with C++
# compilers.
#
# Thus, this Makefile provides two separate targets:
#   - Build     : all-c_tests and all-cpp-tests
#   - Run Tests : run-c-tests and run-cpp-tests
#
# to be able to compile
# \copyright Copyright (c) 2024
# #############################################################################

.DEFAULT_GOAL := all

help::
	@echo ' '
	@echo 'Usage: make <target>'
	@echo ' '
	@echo 'Supported targets:'
	@echo '    all-unit-tests all-c-tests all-cpp-tests all-cc-tests'
	@echo '    run-unit-tests run-c-tests run-cpp-tests run-cc-tests'
	@echo '    clean'
	@echo ' '
	@echo 'To build and run unit-tests:'
	@echo ' make clean && CC=gcc LD=g++ make run-unit-tests'
	@echo ' '
	@echo 'To build C sample-use-case programs and run tests:'
	@echo ' make clean && CC=gcc LD=g++ make all-c-tests'
	@echo ' make run-c-tests'
	@echo ' '
	@echo 'To build *.cpp C++ sample-use-case programs and run tests:'
	@echo ' make clean && CC=g++ CXX=g++ LD=g++ make all-cpp-tests'
	@echo ' make run-cpp-tests'
	@echo ' '
	@echo 'To build *.cc C++ sample-use-case programs and run tests:'
	@echo ' make clean && CC=g++ CXX=g++ LD=g++ make all-cc-tests'
	@echo ' make run-cc-tests'
	@echo ' '
	@echo 'To build client-server performance test programs and run performance test(s)'
	@echo ' make clean && CC=gcc LD=g++ L3_ENABLED=0         make client-server-perf-test  # Baseline'
	@echo ' make clean && CC=gcc LD=g++ L3_LOGT_FPRINTF=1    make client-server-perf-test  # fprintf() logging'
	@echo ' make clean && CC=gcc LD=g++ L3_LOGT_WRITE=1      make client-server-perf-test  # write() msg logging'
	@echo ' make clean && CC=gcc LD=g++                      make client-server-perf-test  # L3-logging'
	@echo ' make clean && CC=gcc LD=g++ L3_FASTLOG_ENABLED=1 make client-server-perf-test  # L3 Fast logging'
	@echo ' make clean && CC=gcc LD=g++ L3_LOC_ENABLED=1     make client-server-perf-test  # L3+LOC logging'
	@echo ' make clean && CC=gcc LD=g++ L3_LOC_ENABLED=2     make client-server-perf-test  # L3+LOC-ELF logging'
	@echo ' '
	@echo 'To build L3-sample programs with LOC-enabled and run unit-tests:'
	@echo ' make clean && CC=gcc LD=g++         L3_LOC_ENABLED=1 make all-c-tests   && L3_LOC_ENABLED=1 make run-c-tests'
	@echo ' make clean && CC=g++ CXX=g++ LD=g++ L3_LOC_ENABLED=1 make all-cpp-tests && L3_LOC_ENABLED=1 make run-cpp-tests'
	@echo ' make clean && CC=g++ CXX=g++ LD=g++ L3_LOC_ENABLED=1 make all-cc-tests  && L3_LOC_ENABLED=1 make run-cc-tests'
	@echo ' '
	@echo 'To build-and-run L3-sample programs with LOC-ELF enabled, directly use the 'run' targets as:'
	@echo ' make clean && CC=gcc LD=g++         L3_LOC_ENABLED=2 make run-c-tests'
	@echo ' make clean && CC=g++ CXX=g++ LD=g++ L3_LOC_ENABLED=2 make run-cpp-tests'
	@echo ' make clean && CC=g++ CXX=g++ LD=g++ L3_LOC_ENABLED=2 make run-cc-tests'
	@echo ' '
	@echo 'To build spdlog:'
	@echo ' make clean && CC=g++ LD=g++ make spdlog-cpp-program'
	@echo ' '
	@echo 'Environment variables: '
	@echo '  BUILD_MODE={release,debug}'
	@echo '  BUILD_VERBOSE={0,1}'
	@echo '  L3_ENABLED={0,1}'
	@echo '  L3_LOC_ENABLED={0,1,2}'
	@echo '  Defaults: CC=gcc CXX=g++ LD=g++'

#
# Verbosity
#
ifndef BUILD_VERBOSE
   BUILD_VERBOSE=0
endif

# Remember if this is being run in CI
ifndef RUN_ON_CI
    RUN_ON_CI=0
endif

# Setup echo formatting for messages.
ifeq "$(BUILD_VERBOSE)" "1"
   COMMAND=
   PROLIX=@echo
   BRIEF=@ >/dev/null echo
   # Always print message describe step executed, even in verbose mode.
   # BRIEF_FORMATTED=@ >/dev/null echo
   BRIEF_FORMATTED=@printf
   BRIEF_PARTIAL=@echo -n >/dev/null
else ifeq "$(BUILD_VERBOSE)" "0"
   COMMAND=@
   PROLIX=@ >/dev/null echo
   BRIEF=@echo
   BRIEF_FORMATTED=@printf
   BRIEF_PARTIAL=@echo -n
else
   $(error Unknown BUILD_VERBOSE mode "$(BUILD_VERBOSE)".  Valid values are "0" or "1". Default is "0")
endif

# Identify the OS we are running on.
UNAME_S := $(shell uname -s)
UNAME_P := $(shell uname -p)

# Compilers to use
CC  ?= gcc
CXX ?= g++
LD  ?= gcc

# ###################################################################
# Symbols for L3 SOURCE DIRECTORIES AND FILES, LOC-Generator Package
# ###################################################################
#
L3PACKAGE   := l3
SRCDIR      := src
INCDIR      := include
L3_SRCDIR   := $(SRCDIR)
L3_INCDIR   := $(INCDIR)
USE_CASES   := use-cases
L3_UTILS    := utils
L3_UTILSDIR := $(USE_CASES)/$(L3_UTILS)

# Name of L3 dump utility that unpacks L3 log-entries
L3_DUMP                 := l3_dump.py
L3_DUMP_ARG_LOG_FILE    := --log-file
L3_DUMP_ARG_BINARY      := --binary

# Unit-tests symbols
TESTS_DIR       := tests
UNIT_DIR        := unit
UNITTESTS_DIR   := $(TESTS_DIR)/$(UNIT_DIR)

# Symbols needed to integrate with LineOfCode (LOC) package
LOC_ROOT        := LineOfCode

# Without need for any generation, LOC-mode used in the LOC package
# is based on LOC-ELF encoding scheme. That scheme requires two
# files that come bundled with that package: src/loc.c, include/loc.h
# Define symbols to access them when building L3 sources with this
# LOC-ELF mode of encoding.
LOC_SRCDIR      := $(LOC_ROOT)/src
LOC_INCDIR      := $(LOC_ROOT)/include
LOCPACKAGE      := $(LOC_ROOT)/loc
LOCGENPY        := $(LOCPACKAGE)/gen_loc_files.py
LOC_ELF_SRC     := $(LOC_SRCDIR)/loc.c

# Names of files generated by LOC Python generator
LOC_FILENAMES   := loc_filenames.c
LOC_HEADER      := loc.h
LOC_TOKENS      := loc_tokens.h

L3_LOC_UNSET            := 0

# -----------------------------------------------------------------------------
# The driving env-var, L3_ENABLED, can be set to one of these values.
# By default, most programs are built with this ON. Only for some performance
# tests, we want to build programs with L3 OFF, then with L3 ON.
# -----------------------------------------------------------------------------
L3_DISABLED     := 0
L3_DEFAULT      := 1

# Set local symbol based on whether environment variable is set.
L3_ENABLED ?= $(L3_DEFAULT)

# -----------------------------------------------------------------------------
# The driving env-var, L3_LOC_ENABLED, needs to be set to one of these values.
# LOC-encoding comes in two flavours. Default technique is based on the
# Python-generator script. Enhanced technique is based on LOC-ELF
# encoding.
# -----------------------------------------------------------------------------
L3_LOC_DEFAULT          := 1
L3_LOC_ELF_ENCODING     := 2

ifndef L3_LOC_ENABLED
    L3_LOC_ENABLED := $(L3_LOC_UNSET)
endif

# As LOC-ELF encoding uses GCC __attribute__ support, we need to always
# compile the relevant .c file from LOC-package using gcc.
# For other build modes, just use CC that user may have specified.
ifeq ($(L3_LOC_ENABLED), $(L3_LOC_DEFAULT))
    LOC_C_CC = $(CC)
else ifeq ($(L3_LOC_ENABLED), $(L3_LOC_ELF_ENCODING))
    LOC_C_CC = gcc
else
    LOC_C_CC = $(CC)
endif

# ###################################################################
# BUILD DIRECTORIES AND FILES
# ###################################################################
#
ifndef BUILD_ROOT
   BUILD_ROOT := build
endif

#
# Build mode
#
ifndef BUILD_MODE
   BUILD_MODE=release
endif
BUILD_DIR := $(BUILD_MODE)

# E.g., will result in one of: `build/release`, `build/debug`
BUILD_PATH=$(BUILD_ROOT)/$(BUILD_DIR)

# Will result in `build/release/obj`, build/debug/obj`
OBJDIR = $(BUILD_PATH)/obj

# Will result in `build/release/bin`, build/debug/bin`
BINDIR = $(BUILD_PATH)/bin

# ###################################################################
# SOURCE DIRECTORIES AND FILES
# ###################################################################

L3_SRC      := $(L3_SRCDIR)/l3.c

# Currently, fast-logging assembly is only supported on x86 64-bit Linux.
ifeq ($(UNAME_S),Linux)
    ifeq ($(UNAME_P), x86_64)
        L3_ASSEMBLY := l3.S
    endif
endif

L3_SRCS     := $(L3_SRC) $(L3_ASSEMBLY)
L3_OBJS     := $(L3_SRCS:%.c=$(OBJDIR)/%.o)

L3_UTILS_SRCS   := $(L3_UTILSDIR)/size_str.c
L3_UTILS_OBJS   := $(L3_UTILS_SRCS:%.c=$(OBJDIR)/%.o)

# Symbol for all unit-test sources, from which we will build standalone
# unit-test binaries.
UNIT_TESTSRC := $(wildcard $(UNITTESTS_DIR)/*.c)

# ###################################################################
# Symbols for test-case data files generated by sample programs
# ###################################################################
TMPDIR              := /tmp
TEST_DATA_SUFFIX    := test.dat

L3_C_DATA           := $(TMPDIR)/$(L3PACKAGE).c-$(TEST_DATA_SUFFIX)
L3_C_SMALL_DATA     := $(TMPDIR)/$(L3PACKAGE).c-small-$(TEST_DATA_SUFFIX)
L3_CPP_DATA         := $(TMPDIR)/$(L3PACKAGE).cpp-$(TEST_DATA_SUFFIX)
L3_CPP_SMALL_DATA   := $(TMPDIR)/$(L3PACKAGE).cpp-small-$(TEST_DATA_SUFFIX)
L3_CC_DATA          := $(TMPDIR)/$(L3PACKAGE).cc-$(TEST_DATA_SUFFIX)
L3_CC_SMALL_DATA    := $(TMPDIR)/$(L3PACKAGE).cc-small-$(TEST_DATA_SUFFIX)

# Name the data file created by unit-test program: l3.c-small-unit-test.dat
L3_C_UNIT_SLOW_LOG_TEST_DATA := $(TMPDIR)/$(L3PACKAGE).c-small-unit-$(TEST_DATA_SUFFIX)
L3_C_UNIT_FAST_LOG_TEST_DATA := $(TMPDIR)/$(L3PACKAGE).c-fast-unit-$(TEST_DATA_SUFFIX)

# ###################################################################
# ---- Symbols to build test-code sample programs
# ###################################################################

# ##############################################################################
# These symbols and lists generate the  dependencies for each use-cases/
# sample program. We have a small collection of C, C++ (.cpp and .cc) sample
# programs as our test "use-cases". In future, more multi-file programs will
# be added to the test-bed, in which case, there will be more than one .o's
# linked to create a use-cases example program.
#
# The goal is to build each sample program as its own binary.
#
# To keep the enumeration easy to follow, the rules for building each binary
# are listed separately.
#
# Every example program of the form bin/use-cases/<eg-prog> depends on
# obj/use-cases/<eg-prog>/*.o -> *.c
# ##############################################################################

# ---- -------------------------------------------------------------------------
# ---- The rules for all stand-alone sample programs follow the same pattern
# ---- as documented here for building the single-file-C-program.
# ---- -------------------------------------------------------------------------
# The top-level dir-name also becomes the name of the resulting binary.
SINGLE_FILE_C_PROGRAM := $(USE_CASES)/single-file-C-program

# Find all sources in that top-level dir. There may be more than one in future.
SINGLE_FILE_C_PROGRAM_SRCS := $(wildcard $(SINGLE_FILE_C_PROGRAM)/*.c)

# Add L3 package's library file to list of sources to be compiled.
SINGLE_FILE_C_PROGRAM_SRCS += $(L3_SRCS)

# Name of LOC-generated source file.
ifeq ($(L3_LOC_ENABLED), $(L3_LOC_DEFAULT))

    SINGLE_FILE_C_PROGRAM_GENSRC := $(SINGLE_FILE_C_PROGRAM)/$(LOC_FILENAMES)
    SINGLE_FILE_C_PROGRAM_SRCS   += $(SINGLE_FILE_C_PROGRAM_GENSRC)

else ifeq ($(L3_LOC_ENABLED), $(L3_LOC_ELF_ENCODING))

    SINGLE_FILE_C_PROGRAM_SRCS   += $(LOC_ELF_SRC)
endif

# Map the list of sources to resulting list-of-objects
SINGLE_FILE_C_PROGRAM_OBJS := $(SINGLE_FILE_C_PROGRAM_SRCS:%.c=$(OBJDIR)/%.o)

# Define a dependency of this sample program's binary to its list of objects
SINGLE_FILE_C_PROGRAM_BIN := $(BINDIR)/$(SINGLE_FILE_C_PROGRAM)
$(SINGLE_FILE_C_PROGRAM_BIN): $(SINGLE_FILE_C_PROGRAM_OBJS)

TEST_C_CODE_BINS := $(SINGLE_FILE_C_PROGRAM_BIN)

ifeq "$(BUILD_VERBOSE)" "1"
    $(info )
    $(info ---- Debug ----)
    $(info $$SINGLE_FILE_C_PROGRAM_SRCS = [ ${SINGLE_FILE_C_PROGRAM_SRCS} ])
    $(info $$SINGLE_FILE_C_PROGRAM_BIN = [ ${SINGLE_FILE_C_PROGRAM_BIN} ])
    $(info ... <- depends on ...)
    $(info $$SINGLE_FILE_C_PROGRAM_OBJS = [ ${SINGLE_FILE_C_PROGRAM_OBJS} ])
    $(info $$TEST_C_CODE_BINS = [ ${TEST_C_CODE_BINS} ])
    $(info )
endif

# ---- -------------------------------------------------------------------------
# ---- Definitions for stand-alone C++ .cpp sample program.
# ---- -------------------------------------------------------------------------
SINGLE_FILE_CPP_PROGRAM := $(USE_CASES)/single-file-Cpp-program

# Find all sources in that top-level dir. There may be more than one in future.
SINGLE_FILE_CPP_PROGRAM_SRCS := $(wildcard $(SINGLE_FILE_CPP_PROGRAM)/*.cpp)

# Add L3 package's library file to list of sources to be compiled.
SINGLE_FILE_CPP_PROGRAM_SRCS += $(L3_SRCS)

# Name of LOC-generated source file.
ifeq ($(L3_LOC_ENABLED), $(L3_LOC_DEFAULT))

    SINGLE_FILE_CPP_PROGRAM_GENSRC := $(SINGLE_FILE_CPP_PROGRAM)/$(LOC_FILENAMES)
    SINGLE_FILE_CPP_PROGRAM_SRCS   += $(SINGLE_FILE_CPP_PROGRAM_GENSRC)

else ifeq ($(L3_LOC_ENABLED), $(L3_LOC_ELF_ENCODING))
    SINGLE_FILE_CPP_PROGRAM_SRCS   += $(LOC_ELF_SRC)
endif

# Map the list of sources to resulting list-of-objects
SINGLE_FILE_CPP_PROGRAM_TMPS := $(SINGLE_FILE_CPP_PROGRAM_SRCS:%.cpp=$(OBJDIR)/%.o)
SINGLE_FILE_CPP_PROGRAM_OBJS := $(SINGLE_FILE_CPP_PROGRAM_TMPS:%.c=$(OBJDIR)/%.o)

# Define a dependency of this sample program's binary to its list of objects
SINGLE_FILE_CPP_PROGRAM_BIN := $(BINDIR)/$(SINGLE_FILE_CPP_PROGRAM)
$(SINGLE_FILE_CPP_PROGRAM_BIN): $(SINGLE_FILE_CPP_PROGRAM_OBJS)

TEST_CPP_CODE_BINS := $(SINGLE_FILE_CPP_PROGRAM_BIN)

ifeq "$(BUILD_VERBOSE)" "1"
    $(info )
    $(info ---- Debug ----)
    $(info $$SINGLE_FILE_CPP_PROGRAM_SRCS = [ ${SINGLE_FILE_CPP_PROGRAM_SRCS} ])
    $(info $$SINGLE_FILE_CPP_PROGRAM_BIN = [ ${SINGLE_FILE_CPP_PROGRAM_BIN} ])
    $(info ... <- depends on ...)
    $(info $$SINGLE_FILE_CPP_PROGRAM_OBJS = [ ${SINGLE_FILE_CPP_PROGRAM_OBJS} ])
    $(info $$TEST_CPP_CODE_BINS = [ ${TEST_CPP_CODE_BINS} ])
    $(info )
endif

# ---- -------------------------------------------------------------------------
# ---- Definitions for stand-alone C++ __LOC__ test .cpp sample program.
# ---- -------------------------------------------------------------------------
LOC_MACRO_TEST_CPP_PROGRAM := $(USE_CASES)/LOC-test-program

# Find all sources in that top-level dir. There may be more than one in future.
LOC_MACRO_TEST_CPP_PROGRAM_SRCS := $(wildcard $(LOC_MACRO_TEST_CPP_PROGRAM)/*.cpp)

# Map the list of sources to resulting list-of-objects
LOC_MACRO_TEST_CPP_PROGRAM_OBJS := $(LOC_MACRO_TEST_CPP_PROGRAM_SRCS:%.cpp=$(OBJDIR)/%.o)

# Define a dependency of this sample program's binary to its list of objects
LOC_MACRO_TEST_CPP_PROGRAM_BIN := $(BINDIR)/$(LOC_MACRO_TEST_CPP_PROGRAM)
$(LOC_MACRO_TEST_CPP_PROGRAM_BIN): $(LOC_MACRO_TEST_CPP_PROGRAM_OBJS)

ifeq ($(L3_LOC_ENABLED), $(L3_LOC_ELF_ENCODING))
    TEST_LOC_MACRO_BINS := $(LOC_MACRO_TEST_CPP_PROGRAM_BIN)
endif

# ---- -------------------------------------------------------------------------
# ---- Definitions for stand-alone C++ .cc sample program.
# ---- -------------------------------------------------------------------------
SINGLE_FILE_CC_PROGRAM := $(USE_CASES)/single-file-CC-program

# Find all sources in that top-level dir. There may be more than one in future.
SINGLE_FILE_CC_PROGRAM_SRCS := $(wildcard $(SINGLE_FILE_CC_PROGRAM)/*.cc)

# Add L3 package's library file to list of sources to be compiled.
SINGLE_FILE_CC_PROGRAM_SRCS += $(L3_SRCS)

# Name of LOC-generated source file.
ifeq ($(L3_LOC_ENABLED), $(L3_LOC_DEFAULT))
    SINGLE_FILE_CC_PROGRAM_GENSRC := $(SINGLE_FILE_CC_PROGRAM)/$(LOC_FILENAMES)
    SINGLE_FILE_CC_PROGRAM_SRCS   += $(SINGLE_FILE_CC_PROGRAM_GENSRC)

else ifeq ($(L3_LOC_ENABLED), $(L3_LOC_ELF_ENCODING))
    SINGLE_FILE_CC_PROGRAM_SRCS   += $(LOC_ELF_SRC)
endif

# Map the list of sources to resulting list-of-objects
SINGLE_FILE_CC_PROGRAM_TMPS := $(SINGLE_FILE_CC_PROGRAM_SRCS:%.cc=$(OBJDIR)/%.o)
SINGLE_FILE_CC_PROGRAM_OBJS := $(SINGLE_FILE_CC_PROGRAM_TMPS:%.c=$(OBJDIR)/%.o)

# Define a dependency of this sample program's binary to its list of objects
SINGLE_FILE_CC_PROGRAM_BIN := $(BINDIR)/$(SINGLE_FILE_CC_PROGRAM)
$(SINGLE_FILE_CC_PROGRAM_BIN): $(SINGLE_FILE_CC_PROGRAM_OBJS)

TEST_CC_CODE_BINS := $(SINGLE_FILE_CC_PROGRAM_BIN)

# ##############################################################################
# Rules to build-and-run spdlog example program.
# ##############################################################################
SPDLOG_EXAMPLE_PROG_DIR     := $(USE_CASES)/spdlog-Cpp-program

SPDLOG_EXAMPLE_PROG_SRCS    := $(wildcard $(SPDLOG_EXAMPLE_PROG_DIR)/*.cpp)

# Map the list of sources to resulting list-of-objects
SPDLOG_EXAMPLE_PROG_OBJS    := $(SPDLOG_EXAMPLE_PROG_SRCS:%.cpp=$(OBJDIR)/%.o)

# Define a dependency of this example program's binary to its list of objects
SPDLOG_EXAMPLE_PROGRAM_BIN  := $(BINDIR)/$(SPDLOG_EXAMPLE_PROG_DIR)
$(SPDLOG_EXAMPLE_PROGRAM_BIN): $(SPDLOG_EXAMPLE_PROG_OBJS)

# We need to effectively execute this build command:
# g++ -I /usr/include/spdlog -o spdlog-Cpp-program test-main.cpp -L ~/Projects/spdlog/build -l spdlog -l fmt
spdlog-cpp-program: CPPFLAGS = --std=c++17

# Define SPD_INCLUDE to work for Linux / Darwin, based on the install tool used.
ifeq ($(UNAME_S),Linux)
    SPD_INCLUDE_ROOT := /usr
else ifeq ($(UNAME_S),Darwin)

    # There seems to be some weirdness when `brew install spdlog` is run on
    # CI machines. On local Mac, this does set-up soft-link as:
    # lld /usr/local/opt/spdlog : /usr/local/opt/spdlog@ -> ../Cellar/spdlog/1.13.0
    #
    # On CI build-machines, seems like this is not being setup. So we have
    # to reset the SPD_INCLUDE_ROOT accordingly, to find the spdlog/ headers.
    #   Found in: /opt/homebrew/Cellar/spdlog/1.13.0/include/spdlog
    ifeq ($(RUN_ON_CI),1)
        # Headers seem to be in: /opt/homebrew/include/spdlog
        SPD_INCLUDE_ROOT := /opt/homebrew
        SPD_LIBS_PATH    := -L /opt/homebrew/lib
    else
        SPD_INCLUDE_ROOT := /usr/local/opt
    endif
endif

SPD_INCLUDE := $(SPD_INCLUDE_ROOT)/include

spdlog-cpp-program: INCLUDE += -I $(SPD_INCLUDE)
spdlog-cpp-program: LIBS += $(SPD_LIBS_PATH) -l spdlog -l fmt
spdlog-cpp-program: $(SPDLOG_EXAMPLE_PROGRAM_BIN)

# ##############################################################################
# Build symbols for single C & C++ unit-test binary that we run
# ##############################################################################
C_UNIT_TEST_BIN     := $(SINGLE_FILE_C_PROGRAM_BIN)
CPP_UNIT_TEST_BIN   := $(SINGLE_FILE_CPP_PROGRAM_BIN)
CC_UNIT_TEST_BIN    := $(SINGLE_FILE_CC_PROGRAM_BIN)
L3_C_UNIT_TEST_BIN  := $(BINDIR)/$(UNIT_DIR)/l3_dump.py-test
LOC_MACRO_TEST_BIN  := $(LOC_MACRO_TEST_CPP_PROGRAM_BIN)
SIZE_UNIT_TEST_BIN  := $(BINDIR)/$(UNIT_DIR)/size_str-test

# L3-logging interfaces' performance unit-tests
FPRINTF_PERF_UNIT_TEST_BIN  := $(BINDIR)/$(UNIT_DIR)/l3-fprintf-perf-test
WRITE_PERF_UNIT_TEST_BIN    := $(BINDIR)/$(UNIT_DIR)/l3-write-perf-test

# ##############################################################################
# Generate symbols and dependencies to build unit-test sources
# ##############################################################################

UNIT_TESTBIN_SRCS := $(filter %-test.c, $(UNIT_TESTSRC))
UNIT_TESTBINS     := $(UNIT_TESTBIN_SRCS:$(TESTS_DIR)/%-test.c=$(BINDIR)/%-test)
ifeq "$(BUILD_VERBOSE)" "1"
    $(info )
    $(info ---- Debug ----)
    $(info $$UNIT_DIR = [ ${UNIT_DIR} ])
    $(info $$UNITTESTS_DIR = [ ${UNITTESTS_DIR} ])
    $(info $$UNIT_TESTBIN_SRCS = [ ${UNIT_TESTBIN_SRCS} ])
    $(info $$UNIT_TESTBINS = [ ${UNIT_TESTBINS} ])
    $(info $$L3_OBJS = [ ${L3_OBJS} ])
    $(info )
endif

# All the stand-alone unit-tests depend on the core L3 file, so add those objects, too.
# Given <x>, this macro generates a line of Makefile of the form
# bin/unit/<x>: obj/unit/<x>.o
define unit_test_self_dependency =
$(1): $(patsubst $(BINDIR)/$(UNIT_DIR)/%,$(OBJDIR)/$(UNITTESTS_DIR)/%.o, $(1)) \
        $(L3_OBJS) $(L3_UTILS_OBJS)
endef

# ---------------------------------------------------------------------------------
# This invocation will generate makefile-snippets specifying the dependency of
# each unit-test binary on its sources and required objects. This way, we do not
# have to enumerate dependency for each unit-test binary built, how it's been
# hand-defined for Mac/OSX. (For some reason the construction of calling the fn
# unit_test_self_dependency() seems to not work on Mac/OSX as it does on Linux.)
#
# See https://www.gnu.org/software/make/manual/html_node/Eval-Function.html
# ---------------------------------------------------------------------------------
ifeq ($(UNAME_S),Linux)

$(OBJDIR)/$(UNITTESTS_DIR)/size_str.o: $(USE_CASES)/$(L3_UTILS)/size_str.c

$(foreach unit,$(UNIT_TESTBINS),$(eval $(call unit_test_self_dependency,$(unit))))

else

$(BINDIR)/$(UNIT_DIR)/l3_dump.py-test: $(OBJDIR)/$(UNITTESTS_DIR)/l3_dump.py-test.o \
                                        $(OBJDIR)/$(SRCDIR)/l3.o

$(BINDIR)/$(UNIT_DIR)/l3-fprintf-perf-test: $(OBJDIR)/$(UNITTESTS_DIR)/l3-fprintf-perf-test.o \
                                            $(OBJDIR)/$(SRCDIR)/l3.o

$(BINDIR)/$(UNIT_DIR)/l3-write-perf-test: $(OBJDIR)/$(UNITTESTS_DIR)/l3-write-perf-test.o \
                                          $(OBJDIR)/$(SRCDIR)/l3.o

endif

# We only need this extra include to find size_str.h, needed to build the
# sources for this unit-test.
$(BINDIR)/$(UNIT_DIR)/size_str-test: INCLUDE += -I ./$(L3_UTILSDIR)
$(BINDIR)/$(UNIT_DIR)/size_str-test: $(OBJDIR)/$(UNITTESTS_DIR)/size_str-test.o \
                                     $(OBJDIR)/$(L3_UTILSDIR)/size_str.o

# Unit-test for l3-write interfaces needs this defined so that l3_log()
# gets correctly redefined to l3_log_fprintf() / l3_log_write() interface
$(BINDIR)/$(UNIT_DIR)/l3-fprintf-perf-test: DFLAGS_UNIT := -DL3_LOGT_FPRINTF
$(BINDIR)/$(UNIT_DIR)/l3-write-perf-test: DFLAGS_UNIT := -DL3_LOGT_WRITE

# ###################################################################
# Report build machine details and compiler version for troubleshooting,
# so we see this output for clean builds, especially in CI-jobs.
# ###################################################################
.PHONY : clean tags
clean:
	uname -a
	$(CC) --version
	rm -rf $(BUILD_ROOT) $(TMPDIR)/$(L3PACKAGE)*$(TEST_DATA_SUFFIX)
	rm -rf $(L3_INCDIR)/loc*.h $(shell find $(USE_CASES) \( -name loc*.h -or -name loc_file*.c \) -print)

# Delete l3.o object as it needs to be recompiled for inclusion w/C++ sources
clean-l3:
	rm -rf $(OBJDIR)/$(SRCDIR)/l3.o

# ##############################################################################
# Make build targets begin here. Make definitions below these targets synthesize
# lists of targets and dependencies that will fulfill this target.
# ##############################################################################

all-c-tests:    $(SINGLE_FILE_C_PROGRAM_GENSRC) $(TEST_C_CODE_BINS)

# ------------------------------------------------------------------------------
# Rule: Use Python generator script to generate the LOC-files
# Rule will be triggered for objects defined to be dependent on $(LOC_FILENAMES)
# sources. Use the triggering target's dir-path to generate .h / .c files
# ------------------------------------------------------------------------------
# NOTE: The generation can be done with one of:
#   a) --gen-includes-dir  $(dir $@) - which will generate the loc*.h files in the
#           dir for the sample-program's sources, or
#   b) --gen-includes-dir  $(L3_INCDIR) - which will generate the loc*.h files in
#           common include/ dir.
#
# We do (b) as src/l3.c also needs access to the LOC-definitions, which it
# can't get to if these LOC headers were generated in the sample-program's dir.
#
# Additionally, generate using:
#
#   c) --loc-decoder-dir, to specify the dir where the LOC-decoder binary,
#      specific to the sample program being built, should be generated.
#      (The l3_dump.py will later look for this LOC-decoder binary
#       to unpack the LOC-ID value.)
#
#      To make this work, specify a dependency of the generated-sources on the
#      output bin/ dir that is specified for --loc-decoder-dir, below.
#      The .SECONDEXPANSION specified later on will ensure that the output dir
#      gets created -before- the LOC-Python generator script is invoked.
#
$(SINGLE_FILE_C_PROGRAM_GENSRC): | $(BINDIR)/$(USE_CASES)/.
	@echo
	@echo "Invoke LOC-generator triggered by: " $@
	@$(LOCGENPY) --gen-includes-dir  $(L3_INCDIR) --gen-source-dir $(dir $@) --src-root-dir $(dir $@) --loc-decoder-dir $(BINDIR)/$(USE_CASES) --verbose
	@echo

all-cpp-tests:  $(SINGLE_FILE_CPP_PROGRAM_GENSRC) $(TEST_CPP_CODE_BINS)

$(SINGLE_FILE_CPP_PROGRAM_GENSRC): | $(BINDIR)/$(USE_CASES)/.
	@echo
	@echo "Invoke LOC-generator triggered by: " $@
	@$(LOCGENPY) --gen-includes-dir  $(L3_INCDIR) --gen-source-dir $(dir $@) --src-root-dir $(dir $@) --loc-decoder-dir $(BINDIR)/$(USE_CASES) --verbose
	@echo

all-cc-tests:  $(SINGLE_FILE_CC_PROGRAM_GENSRC) $(TEST_CC_CODE_BINS)

$(SINGLE_FILE_CC_PROGRAM_GENSRC): | $(BINDIR)/$(USE_CASES)/.
	@echo
	@echo "Invoke LOC-generator triggered by: " $@
	@$(LOCGENPY) --gen-includes-dir  $(L3_INCDIR) --gen-source-dir $(dir $@) --src-root-dir $(dir $@) --loc-decoder-dir $(BINDIR)/$(USE_CASES) --verbose
	@echo

all-loc-tests: $(TEST_LOC_MACRO_BINS)

all-unit-tests: $(UNIT_TESTBINS)

# NOTE: We cannot easily support 'all' target as we have to juggle between
#       use of different compilers, gcc, g++ etc. That became difficult to
#       specify for different build rules.
# all: all-c-tests all-cpp-tests all-cc-tests all-unit-tests all-loc-tests

# ##############################################################################
# Rules to build-and-run Client-Server message exchange performance tests.
# ##############################################################################

CLIENT_SERVER_PERF_TESTS_DIR   := $(USE_CASES)/client-server-msgs-perf

# Symbol for all client-server messaging performance test sources, from which
# we will build standalone client and server binaries.
CLIENT_SERVER_PERF_TESTS_SRCS   := $(wildcard $(CLIENT_SERVER_PERF_TESTS_DIR)/*.c)

CLIENT_SERVER_PERF_TESTS_SRCS   += $(L3_UTILS_SRCS)

# Build pair of sources for client-/server-main sources.
CLIENT_SERVER_CLIENT_MAIN_SRC   := $(filter %_client.c, $(CLIENT_SERVER_PERF_TESTS_SRCS))
CLIENT_SERVER_SERVER_MAIN_SRC   := $(filter %_server.c, $(CLIENT_SERVER_PERF_TESTS_SRCS))

# Build list of sources other than client-/server-main sources.
CLIENT_SERVER_NON_MAIN_SRCS     := $(filter-out $(CLIENT_SERVER_PERF_TESTS_DIR)/svmsg_file_%.c, $(CLIENT_SERVER_PERF_TESTS_SRCS))

# If L3-logging is enabled in test-program, need to include core library file
ifeq ($(L3_ENABLED), $(L3_DEFAULT))
    CLIENT_SERVER_NON_MAIN_SRCS += $(L3_SRC)

# Under L3-logging, invoke the L3-fast logging API, which needs the .S file
ifeq ($(L3_FASTLOG_ENABLED), 1)

    CLIENT_SERVER_NON_MAIN_SRCS += $(L3_ASSEMBLY)
    CFLAGS += -DL3_FASTLOG_ENABLED

else ifeq ($(L3_LOGT_FPRINTF), 1)

    CFLAGS += -DL3_LOGT_FPRINTF

else ifeq ($(L3_LOGT_WRITE), 1)

    CFLAGS += -DL3_LOGT_WRITE

endif

endif   # L3_ENABLED

# Name of LOC-generated source file for the client-server perf-test program.
ifeq ($(L3_LOC_ENABLED), $(L3_LOC_DEFAULT))

    CLIENT_SERVER_PROGRAM_GENSRC := $(CLIENT_SERVER_PERF_TESTS_DIR)/$(LOC_FILENAMES)
    CLIENT_SERVER_NON_MAIN_SRCS  += $(CLIENT_SERVER_PROGRAM_GENSRC)

else ifeq ($(L3_LOC_ENABLED), $(L3_LOC_ELF_ENCODING))

    # If L3-LOC_ELF encoding is chosen, link with system-provided loc.c
    CLIENT_SERVER_NON_MAIN_SRCS += $(LOC_ELF_SRC)

endif

# Map the list of sources to resulting list-of-objects
CLIENT_SERVER_CLIENT_MAIN_OBJ   := $(CLIENT_SERVER_CLIENT_MAIN_SRC:%.c=$(OBJDIR)/%.o)
CLIENT_SERVER_SERVER_MAIN_OBJ   := $(CLIENT_SERVER_SERVER_MAIN_SRC:%.c=$(OBJDIR)/%.o)
CLIENT_SERVER_NON_MAIN_OBJS     := $(CLIENT_SERVER_NON_MAIN_SRCS:%.c=$(OBJDIR)/%.o)

# This client-server test involves a client- and server-driver program,
# named as 'svmsg_file_*.c'. Filter accordingly.
CLIENT_SERVER_PERF_TEST_BIN_SRCS := $(filter $(CLIENT_SERVER_PERF_TESTS_DIR)/svmsg_file_%.c, $(CLIENT_SERVER_PERF_TESTS_SRCS))

# Define symbols for respective client/server binaries in build area.
# Replace 'use-cases/client-server-msgs-perf/svmsg_file_client.c'
#      -> 'build/release/bin/use-cases/svmsg_file_client
#
CLIENT_SERVER_PERF_TEST_BINS    := $(CLIENT_SERVER_PERF_TEST_BIN_SRCS:$(CLIENT_SERVER_PERF_TESTS_DIR)/%.c=$(BINDIR)/$(USE_CASES)/%)

ifeq "$(BUILD_VERBOSE)" "1"
    $(info )
    $(info ---- Debug ----)
    $(info $$L3_ENABLED = [ ${L3_ENABLED} ])
    $(info $$CLIENT_SERVER_PERF_TESTS_DIR = [ ${CLIENT_SERVER_PERF_TESTS_DIR} ])
    $(info $$CLIENT_SERVER_PERF_TESTS_SRCS = [ ${CLIENT_SERVER_PERF_TESTS_SRCS} ])
    $(info $$CLIENT_SERVER_PERF_TEST_BIN_SRCS = [ ${CLIENT_SERVER_PERF_TEST_BIN_SRCS} ])
    $(info $$CLIENT_SERVER_PERF_TEST_BINS = [ ${CLIENT_SERVER_PERF_TEST_BINS} ])
    $(info $$CLIENT_SERVER_NON_MAIN_OBJS = [ ${CLIENT_SERVER_NON_MAIN_OBJS} ])
endif

$(BINDIR)/$(USE_CASES)/svmsg_file_client: $(CLIENT_SERVER_CLIENT_MAIN_OBJ) $(CLIENT_SERVER_NON_MAIN_OBJS)
$(BINDIR)/$(USE_CASES)/svmsg_file_server: $(CLIENT_SERVER_SERVER_MAIN_OBJ) $(CLIENT_SERVER_NON_MAIN_OBJS)

$(CLIENT_SERVER_PROGRAM_GENSRC): | $(BINDIR)/$(USE_CASES)/.
	@echo
	@echo "Invoke LOC-generator triggered by: " $@
	@$(LOCGENPY) --gen-includes-dir  $(L3_INCDIR) --gen-source-dir $(dir $@) --src-root-dir $(dir $@) --loc-decoder-dir $(BINDIR)/$(USE_CASES) --verbose
	@echo

client-server-perf-test: INCLUDE += -I ./$(L3_UTILSDIR)
client-server-perf-test: $(CLIENT_SERVER_PROGRAM_GENSRC) $(CLIENT_SERVER_PERF_TEST_BINS)

# ##############################################################################
# CFLAGS, LDFLAGS, ETC
# ##############################################################################
#

# -----------------------------------------------------------------------------
# Define the include files' dir-path.
# -----------------------------------------------------------------------------
INCLUDE = -I ./$(L3_INCDIR)
INCLUDE += -I ./$(dir $<)

ifeq ($(L3_LOC_ENABLED), $(L3_LOC_ELF_ENCODING))
    INCLUDE += -I ./$(LOC_INCDIR)
endif

# -----------------------------------------------------------------------------
# Ref: https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
#   -Ofast: Disregard strict standards compliance. Enables all -O3 optimizations.
#   -Og   : Optimize debugging experience. Should be the optimization level of
#           choice for the standard edit-compile-debug cycle.
# use += here, so that extra flags can be provided via the environment
# -----------------------------------------------------------------------------
ifeq "$(BUILD_MODE)" "debug"
    CFLAGS += -DDEBUG -Og
else ifeq "$(BUILD_MODE)" "release"
     CFLAGS += -Ofast
endif

# By default, L3-logging is always ON for all programs built here.
# So, the -DL3_ENABLED flag is really just a documentation pass-through.
# Except, some performance benchmarking programs are built with L3 OFF to
# compare the performance of the benchmark with L3 ON.
ifeq ($(L3_ENABLED), $(L3_DEFAULT))
      CFLAGS += -DL3_ENABLED
endif

# By default, L3-logging runs on its own. To integrate L3 with LOC machinery,
# to squirrel away the loc_t ID value, run: L3_LOC_ENABLED=1 make ...
ifeq ($(L3_LOC_ENABLED), $(L3_LOC_DEFAULT))

    CFLAGS += -DL3_LOC_ENABLED
    CFLAGS += -DLOC_FILE_INDEX=LOC_$(subst .,_,$(subst -,_,$(notdir $<)))
    LDFLAGS += -DL3_LOC_ENABLED

else ifeq ($(L3_LOC_ENABLED), $(L3_LOC_ELF_ENCODING))

    # Core L3 files flag off of L3_LOC_ENABLED. L3_LOC_ELF_ENABLED is provided
    # in case in some sources we wish to conditionally compile some info-msgs
    # indicating that this LOC-ELF encoding scheme is in effect.
    CFLAGS += -DL3_LOC_ENABLED -DL3_LOC_ELF_ENABLED

    LDFLAGS += -DL3_LOC_ENABLED

endif

CFLAGS += -D_GNU_SOURCE -ggdb3 -Wall -Wfatal-errors -Werror

CPPFLAGS += --std=c++11

LIBS += -ldl

# ##############################################################################
# Automatically create directories, based on
# http://ismail.badawi.io/blog/2017/03/28/automatic-directory-creation-in-make/
# ##############################################################################
#
.SECONDEXPANSION:

.SECONDARY:

%/.:
	$(COMMAND) mkdir -p $@

# These targets prevent circular dependencies arising from the
# recipe for building binaries
$(BINDIR)/.:
	$(COMMAND) mkdir -p $@

$(BINDIR)/%/.:
	$(COMMAND) mkdir -p $@

# ##############################################################################
# RECIPES:
# ##############################################################################
#
# For all-test-code, we need to use -I test-code/<subdir>
# Dependencies for the main executables
COMPILE.c       = $(CC)  -x c $(CFLAGS) $(DFLAGS_UNIT) $(INCLUDE) -c
COMPILE.cpp     = $(CXX) -x c++ $(CPPFLAGS) $(CFLAGS) $(INCLUDE) -c
COMPILE.cc      = $(CXX) -x c++ $(CFLAGS) $(INCLUDE) -c
COMPILE.loc.c   = $(LOC_C_CC) $(CFLAGS) $(INCLUDE) -c

# Compile each .c file into its .o
# Also define a dependency on the dir in which .o will be produced (@D).
# The secondary expansion will invoke mkdir to create output dirs first.
$(OBJDIR)/LineOfCode/src/loc.o: $(LOC_ELF_SRC) | $$(@D)/.
	$(BRIEF_FORMATTED) "%-20s %-50s [%s]\n" Compiling $< $@
	$(COMMAND) $(COMPILE.loc.c) $< -o $@
	$(PROLIX) # blank line

$(OBJDIR)/%.o: %.c | $$(@D)/.
	$(BRIEF_FORMATTED) "%-20s %-50s [%s]\n" Compiling $< $@
	$(COMMAND) $(COMPILE.c) $< -o $@
	$(PROLIX) # blank line

# Compile each .cpp file into its .o
$(OBJDIR)/%.o: %.cpp | $$(@D)/.
	$(BRIEF_FORMATTED) "%-20s %-50s [%s]\n" Compiling $< $@
	$(COMMAND) $(COMPILE.cpp) $< -o $@
	$(PROLIX) # blank line

# Compile each .cc file into its .o
$(OBJDIR)/%.o: %.cc | $$(@D)/.
	$(BRIEF_FORMATTED) "%-20s %-50s [%s]\n" Compiling $< $@
	$(COMMAND) $(COMPILE.cpp) $< -o $@
	$(PROLIX) # blank line

# Link .o's to produce running binary
# Define dependency on output dir existing, so secondary expansion will
# trigger mkdir to create bin/s output dir.
# If you add "$^" to 'Linking' message, you will see list of .o's being linked
$(BINDIR)/%: | $$(@D)/.
	$(BRIEF_FORMATTED) "%-20s %s\n" Linking $@
	$(COMMAND) $(LD) $(LDFLAGS) $^ -o $@ $(LIBS)
	$(PROLIX) # blank line

#*************************************************************#
# Testing: Make test targets
#*************************************************************#
#

.PHONY: install

# NOTE: We cannot easily support 'all' target as we have to juggle between
#       use of different compilers, gcc, g++ etc. That became difficult to
#       specify for different build rules.
# run-tests: run-c-tests run-cpp-tests run-cc-tests run-unit-tests

run-c-tests: all-c-tests
	@echo
	@echo '---- Run C unit-tests: ----'
	./$(C_UNIT_TEST_BIN)
	@echo
	python3 $(L3_DUMP) $(L3_DUMP_ARG_LOG_FILE) $(L3_C_DATA) $(L3_DUMP_ARG_BINARY) ./$(C_UNIT_TEST_BIN)
	@echo
	python3 $(L3_DUMP) $(L3_DUMP_ARG_LOG_FILE) $(L3_C_SMALL_DATA) $(L3_DUMP_ARG_BINARY) ./$(C_UNIT_TEST_BIN)

run-cpp-tests: all-cpp-tests
	@echo
	@echo '---- Run C++ .cpp unit-tests: ----'
	./$(CPP_UNIT_TEST_BIN)
	@echo
	python3 $(L3_DUMP) $(L3_DUMP_ARG_LOG_FILE) $(L3_CPP_DATA) $(L3_DUMP_ARG_BINARY) ./$(CPP_UNIT_TEST_BIN)
	@echo
	python3 $(L3_DUMP) $(L3_DUMP_ARG_LOG_FILE) $(L3_CPP_SMALL_DATA) $(L3_DUMP_ARG_BINARY) ./$(CPP_UNIT_TEST_BIN)

run-cc-tests: all-cc-tests
	@echo
	@echo '---- Run Cpp *.cc unit-tests: ----'
	./$(CC_UNIT_TEST_BIN)
	@echo
	python3 $(L3_DUMP) $(L3_DUMP_ARG_LOG_FILE) $(L3_CC_DATA) $(L3_DUMP_ARG_BINARY) ./$(CC_UNIT_TEST_BIN)
	@echo
	python3 $(L3_DUMP) $(L3_DUMP_ARG_LOG_FILE) $(L3_CC_SMALL_DATA) $(L3_DUMP_ARG_BINARY) ./$(CC_UNIT_TEST_BIN)

run-unit-tests: all-unit-tests
	@echo
	@echo '---- Run L3 unit-tests: ----'
	./$(L3_C_UNIT_TEST_BIN)
	@echo
	python3 $(L3_DUMP) $(L3_DUMP_ARG_LOG_FILE) $(L3_C_UNIT_SLOW_LOG_TEST_DATA) $(L3_DUMP_ARG_BINARY) ./$(L3_C_UNIT_TEST_BIN)
	@echo
	python3 $(L3_DUMP) $(L3_DUMP_ARG_LOG_FILE) $(L3_C_UNIT_FAST_LOG_TEST_DATA) $(L3_DUMP_ARG_BINARY) ./$(L3_C_UNIT_TEST_BIN)
	@echo
	./$(SIZE_UNIT_TEST_BIN)
	@echo
	./$(FPRINTF_PERF_UNIT_TEST_BIN)
	# L3-write performance test seems to work better on subsequent runs.
	@echo
	./$(WRITE_PERF_UNIT_TEST_BIN)
	@echo
	./$(WRITE_PERF_UNIT_TEST_BIN)

run-loc-tests: all-loc-tests
	@echo
	@echo '---- Run LOC unit-test: ----'
	./$(LOC_MACRO_TEST_BIN)
