### GNU Makefile build file

# Compiler Settings

STD = --std=gnu99

WARN_FLAGS = -Werror \
-Wpedantic \
-Wall \
-Wextra \
-Wbad-function-cast \
-Wcast-align \
-Wcast-qual \
-Wfloat-equal \
-Wformat=2 \
-Wlogical-op \
-Wnested-externs \
-Wpointer-arith \
-Wundef \
-Wno-pointer-compare \
-Wredundant-decls \
-Wsequence-point \
-Wshadow \
-Wstrict-prototypes \
-Wswitch \
-Wundef \
-Wunreachable-code \
-Wunused-but-set-parameter \
-Wwrite-strings

# Project-specific settings

INCLUDES = -I include/
DEFINES ?= -DNOTICEBOARD_SOCK_NAME=\"noticeboard.sock\" -DNOTICEBOARD_DIR_NAME=\"noticeboard_notes/\" -DNOTICEBOARD_ROOT_DIR_NAME=\".\"
OTHER_FLAGS = -g

all: communication server client

.PHONY: all

communication:
	@echo "\033[0;35m""Building communication library" "\033[0m"
	cc $(STD) $(WARN_FLAGS) $(OTHER_FLAGS) $(INCLUDES) $(DEFINES) -c src/request.c -o lib/request.o
	cc $(STD) $(WARN_FLAGS) $(OTHER_FLAGS) $(INCLUDES) $(DEFINES) -c src/response.c -o lib/response.o

server: communication
	@echo "\033[0;35m""Building server library" "\033[0m"
	cc $(STD) $(WARN_FLAGS) $(OTHER_FLAGS) $(INCLUDES) $(DEFINES) -c src/client_handling.c -o lib/client_handling.o
	cc $(STD) $(WARN_FLAGS) $(OTHER_FLAGS) $(INCLUDES) $(DEFINES) -c src/server.c -o lib/server.o
	@echo "\033[0;35m""Generating server executable" "\033[0m"
	cc $(STD) $(WARN_FLAGS) $(OTHER_FLAGS) $(INCLUDES) $(DEFINES) lib/request.o lib/response.o lib/client_handling.o lib/server.o -o bin/noticeboard

client: communication
	@echo "\033[0;35m""Building client library" "\033[0m"
	cc $(STD) $(WARN_FLAGS) $(OTHER_FLAGS) $(INCLUDES) $(DEFINES) -c src/client.c -o lib/client.o
	@echo "\033[0;35m""Generating client executable" "\033[0m"
	cc $(STD) $(WARN_FLAGS) $(OTHER_FLAGS) $(INCLUDES) $(DEFINES) lib/request.o lib/response.o lib/client.o -o bin/note

clean:
	@echo "\033[0;35m""Cleaning libs and exes" "\033[0m"
	rm lib/* bin/* || true
