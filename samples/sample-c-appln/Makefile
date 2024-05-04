################################################################################
### CUSTOMIZE BELOW HERE #######################################################

PROJECT_ROOT_DIR := .

# Define the compiler to use
CC          := gcc

# Define the name of the executable
TARGET      := c-sample

# List of project source files
SOURCES     := main.c functions.c

CFLAGS      := -O3

INCLUDE += -I $(PROJECT_ROOT_DIR)

################################################################################
### DO NOT EDIT THE FOLLOWING LINES ############################################

# Define list of objects
OBJSC   := $(SOURCES:.c=.o)
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
	rm -f $(OBJS)

################################################################################
################################################################################
