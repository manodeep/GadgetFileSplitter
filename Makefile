CC:=gcc

TARGET:=gadget_file_splitter
SRC:=main.c filesplitter.c utils.c handle_gadget.c split_gadget.c progressbar.c
OBJS:=$(SRC:.c=.o)
INCL:=filesplitter.h utils.h handle_gadget.h gadget_defs.h split_gadget.h progressbar.h

OPTIONS:=
OPTIMIZE:=-O3 -march=native
CCFLAGS:=-Wall -Wextra -Wshadow -g -m64 -std=gnu99 -D_LARGEFILE64_SOURCE=1 -D_FILE_OFFSET_BITS=64
CCFLAGS += -Wsign-compare -fPIC -D_POSIX_SOURCE=200809L -D_GNU_SOURCE -D_DARWIN_C_SOURCE
CCFLAGS +=-fno-strict-aliasing
CCFLAGS += -Wformat=2  -Wpacked  -Wnested-externs -Wpointer-arith  -Wredundant-decls  -Wfloat-equal -Wcast-qual
CCFLAGS += -Wcast-align -Wmissing-declarations -Wmissing-prototypes  -Wnested-externs -Wstrict-prototypes -Wpadded #-Wconversion

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
