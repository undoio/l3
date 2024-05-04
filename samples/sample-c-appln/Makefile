################################################################################
### CUSTOMIZE BELOW HERE #######################################################

PROJECT_ROOT_DIR := .

# Define the compiler to use
CC          := gcc

# Define the name of the executable
TARGET      := c-sample

L3_ROOTDIR  := ../..
L3_SRCDIR   := $(L3_ROOTDIR)/src
L3_INCDIR   := $(L3_ROOTDIR)/include
L3_SRC      := $(L3_SRCDIR)/l3.c

# Hard-coded name of L3 log-file created in main(). This is not really needed
# for builds to work, but is made visible here for documentation purposes.
L3_LOG_FILE := /tmp/c-sample-test.dat

# List of project source files
SOURCES     := main.c functions.c

CFLAGS      := -O3

# Also include L3's include files' dir-path.
INCLUDE += -I $(PROJECT_ROOT_DIR) -I ./$(L3_INCDIR)

DFLAGS  :=  -D_GNU_SOURCE

################################################################################
### DO NOT EDIT THE FOLLOWING LINES ############################################

# Define list of objects
OBJSC   := $(SOURCES:.c=.o)
OBJSC   += $(L3_SRC)
OBJS    := $(OBJSC:.c=.o)

# the target is obtained linking all .o files
all: $(SOURCES) $(TARGET)

# Compile each .c file into its .o
%.o: %.c
	$(CC) $(INCLUDE) $(DFLAGS) -o $@ -c $<

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)
	./$(TARGET)

purge: clean
	rm -f $(TARGET)

# Delete all objects compiled.
clean:
	rm -f $(OBJS) $(L3_LOG_FILE)

################################################################################
