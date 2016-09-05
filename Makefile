CC:=gcc

TARGET:=gadget_file_splitter
SRC:=main.c filesplitter.c utils.c handle_gadget.c
OBJS:=$(SRC:.c=.o)
INCL:=filesplitter.h utils.h handle_gadget.h gadget_defs.h

OPTIONS:=
OPTIMIZE:=-O3
CCFLAGS:=-Wall -Wextra -Wshadow -g -m64 -std=gnu99 -D_LARGEFILE64_SOURCE=1 -D_FILE_OFFSET_BITS=64
CLINK:=-lm

all: $(TARGET) $(SRC) $(INCL) Makefile

%.o:%.c Makefile
	$(CC) $(CCFLAGS) $(OPTIONS) $(OPTIMIZE) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CLINK) $^ -o $@

.phony: clena celan celna clean
clena celan celna: clean
clean:
	$(RM) $(OBJS) $(TARGET)
