################################################################################
# Makefile for the sample-c-appln sources.
#
# This set of files has been cloned from the `SimpleMakefile`
# (https://github.com/lostella/SimpleMakefile) repo as a starting project
# source-base. Those sources have been extended to show the required  Makefile
# changes required to integrate existing project sources with the L3/LOC packages.
#
# The efforts of the owner of the SimpleMakefile repo, Lorenzo Stella, are
# gratefully appreciated!
#
### CUSTOMIZE BELOW HERE #######################################################

PROJECT_ROOT_DIR := .

# Identify the OS we are running on.
UNAME_S     := $(shell uname -s)
UNAME_P     := $(shell uname -p)

# Define the compiler to use
CC          := gcc

# Define the name of the executable
TARGET      := c-sample

L3_ROOTDIR  := ../..
L3_SRCDIR   := $(L3_ROOTDIR)
L3_INCDIR   := $(L3_ROOTDIR)
L3_SRC      := $(L3_SRCDIR)/l3.c

# Currently, fast-logging assembly is only supported on x86 64-bit Linux.
ifeq ($(UNAME_S),Linux)
    ifeq ($(UNAME_P), x86_64)
        L3_ASSEMBLY := $(L3_ROOTDIR)/l3.S
    endif
endif

# Hard-coded name of L3 log-file created in main(). This is not really needed
# for builds to work, but is made visible here for documentation purposes.
L3_LOG_FILE := /tmp/c-sample-test.dat

# List of project source files
SOURCES     := main.c functions.c

CFLAGS       = -O3

LIBS        += -ldl

# Also include L3's include files' dir-path.
INCLUDE += -I $(PROJECT_ROOT_DIR) -I ./$(L3_INCDIR)

DFLAGS  :=  -D_GNU_SOURCE

# Define symbols to access LOC-package related artifacts
L3_LOC_ROOTDIR      := $(L3_ROOTDIR)/LineOfCode
LOC_INCDIR          := $(L3_LOC_ROOTDIR)/include
LOC_SRCDIR          := $(L3_LOC_ROOTDIR)/src
LOC_FILENAMES       := loc_filenames.c
LOC_GENPY           := $(L3_LOC_ROOTDIR)/loc/gen_loc_files.py
LOC_GENFILES_DIR    := $(PROJECT_ROOT_DIR)

# The package-provided LOC source file that provides ELF-section
# based LOC-ELF encoding. Needs to be linked with user sources.
LOC_ELF_SRC         := $(LOC_SRCDIR)/loc.c

# -----------------------------------------------------------------------------
# The driving env-var, L3_LOC_ENABLED, needs to be set to one of these values.
# LOC-encoding comes in two flavours. Default technique is based on the
# Python-generator script. Enhanced technique is based on LOC-ELF
# encoding.
# -----------------------------------------------------------------------------
L3_LOC_UNSET            := 0
L3_LOC_DEFAULT          := 1
L3_LOC_ELF_ENCODING     := 2

ifndef L3_LOC_ENABLED
    L3_LOC_ENABLED := $(L3_LOC_UNSET)
endif

# By default, L3-logging runs on its own. To integrate L3 with LOC machinery,
# to squirrel away the loc_t ID value, run: L3_LOC_ENABLED=1 make ...
# -DL3_LOC_ENABLED is required to activate conditional code in various sources
ifeq ($(L3_LOC_ENABLED), $(L3_LOC_DEFAULT))

    DFLAGS += -DL3_LOC_ENABLED
    CFLAGS += -DLOC_FILE_INDEX=LOC_$(subst .,_,$(subst -,_,$(notdir $<)))
    LDFLAGS += -DL3_LOC_ENABLED

else ifeq ($(L3_LOC_ENABLED), $(L3_LOC_ELF_ENCODING))

    # We need L3_LOC_ELF_ENABLED to record, in the L3 log-header, the type
    # of LOC-encoding in-effect for this build.
    DFLAGS  += -DL3_LOC_ENABLED -DL3_LOC_ELF_ENABLED
    INCLUDE += -I $(LOC_INCDIR)

endif

################################################################################
### DO NOT EDIT THE FOLLOWING LINES ############################################

# Define list of objects
OBJSC   := $(SOURCES:.c=.o)
OBJSC   += $(L3_SRC)

# Define and include name of LOC-generated source file.
ifeq ($(L3_LOC_ENABLED), $(L3_LOC_DEFAULT))

    C_SAMPLES_LOC_GENSRC := $(PROJECT_ROOT_DIR)/$(LOC_FILENAMES)
    OBJSC                += $(C_SAMPLES_LOC_GENSRC)

else ifeq ($(L3_LOC_ENABLED), $(L3_LOC_ELF_ENCODING))

    # Pull-in package-provided LOC source file.
    OBJSC                += $(LOC_ELF_SRC)

endif

OBJS    := $(OBJSC:.c=.o)

# Rule to invoke the LOC-Python generator to produce required .h/.c files
$(C_SAMPLES_LOC_GENSRC): | $(BINDIR)/$(USE_CASES)/.
	@echo
	@echo "Invoke LOC-generator triggered by: " $@
	@$(LOC_GENPY) --src-root-dir $(dir $@) --gen-includes-dir $(LOC_GENFILES_DIR) --gen-source-dir $(LOC_GENFILES_DIR) --loc-decoder-dir $(PROJECT_ROOT_DIR) --verbose
	mv $(PROJECT_ROOT_DIR)/._loc $(PROJECT_ROOT_DIR)/c-sample_loc
	@echo

# the target is obtained linking all .o files
all: $(C_SAMPLES_LOC_GENSRC) $(SOURCES) $(TARGET)

# Compile each .c file into its .o
%.o: %.c
	$(CC) $(CFLAGS) $(DFLAGS) $(INCLUDE) -o $@ -c $<

$(TARGET): $(C_SAMPLES_LOC_GENSRC) $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) $(LDFLAGS) $(L3_ASSEMBLY) -o $(TARGET) $(LIBS)
	./$(TARGET)

purge: clean
	rm -f $(TARGET)

# Delete all generated sources and objects compiled.
# Also delete any stale loc*.h files that may have been left behind in L3's include/ dir
clean:
	rm -f $(TARGET) $(OBJS) $(L3_LOG_FILE) loc*.h loc*.c loc*.o $(L3_INCDIR)/loc*.h

################################################################################
