CC:=mpicc

TARGET:=gadget_file_splitter
SRC:=main.c filesplitter.c utils.c handle_gadget.c split_gadget.c progressbar.c mpi_wrapper.c
OBJS:=$(SRC:.c=.o)
INCL:=filesplitter.h utils.h handle_gadget.h gadget_defs.h split_gadget.h progressbar.h mpi_wrapper.h

OPTIONS:= # -DUSE_MPI
OPTIMIZE:=-O3 -march=native
CCFLAGS:= 

include common.mk

all: $(TARGET) $(SRC) $(INCL) Makefile

%.o:%.c Makefile common.mk $(INCL)
	$(CC) $(OPTIONS) $(CCFLAGS) $(OPTIMIZE) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CLINK) $^ -o $@

.phony: clena celan celna clean
clena celan celna: clean
clean:
	$(RM) $(OBJS) $(TARGET)
