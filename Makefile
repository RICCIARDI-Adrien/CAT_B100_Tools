CC = gcc
CFLAGS += -W -Wall

BINARY = b100-tools
SOURCES = $(wildcard Sources/*.c)

all:
	$(CC) $(CFLAGS) -I Submodules/Serial_Port_Library/Includes -I Includes Submodules/Serial_Port_Library/Sources/Serial_Port_Linux.c $(SOURCES) -o $(BINARY)
