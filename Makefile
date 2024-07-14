CC = gcc
CFLAGS += -W -Wall

BINARY = b100-tools
INCLUDES = -I Submodules/Serial_Port_Library/Includes -I Includes
SOURCES = $(wildcard Sources/*.c)

all:
	$(CC) $(CFLAGS) $(INCLUDES) Submodules/Serial_Port_Library/Sources/Serial_Port_Linux.c $(SOURCES) -o $(BINARY)

debug: CFLAGS += -g
debug: all

cppcheck:
	cppcheck --check-level=exhaustive $(INCLUDES) Sources
