ifndef BIN_DIR
BIN_DIR := $(CURDIR)
endif

CC        = g++
EXEC      = e-mem
CFLAGS    = -Wall -Wextra -Wunused -mpopcnt -std=gnu++0x -fopenmp 
CDEBUG    = -g -ggdb -DDEBUG 
CPROF    = -g -ggdb -DDEBUG -pg 
COPTIMIZE = -Wuninitialized -O3 -fomit-frame-pointer
CLIBS     = -lm

CSRCS     = $(wildcard *.cpp)
CHDRS     = $(wildcard *.h)
TXTS      = $(wildcard *.txt README* LICENSE*)
SCRIPTS   = $(wildcard Makefile* *.sh *.py)

NAME    := "e-mem"
CPUARCH := $(shell uname -m)

ifeq ($(MAKECMDGOALS),debug)
        CFLAGS += $(CDEBUG)
else ifeq ($(MAKECMDGOALS),profile)
        CFLAGS += $(CPROF)
else
        CFLAGS += $(COPTIMIZE)
endif

.PHONY: all clean pack bin

all: clean bin

debug: all

profile: all

bin:
	@echo :: Compiling \"$(NAME)\" \($(CPUARCH)\) ...
	$(CC) $(CFLAGS) $(CSRCS) $(CLIBS) -o $(BIN_DIR)/$(EXEC) 
	chmod 755 $(BIN_DIR)/$(EXEC)
	@echo :: Done

clean:
	@echo :: Cleaning up ...
	@rm -f $(BIN_DIR)/$(EXEC) $(BIND_DIR)/$(EXEC).tar.gz

pack:
	@echo :: Packing files ...
	tar -cvzhf $(BIND_DIR)/$(EXEC).tar.gz $(CSRCS) $(CHDRS) $(TXTS) $(SCRIPTS)
	@echo :: Done

