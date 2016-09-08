CC:=mpicc

TARGET:=gadget_file_splitter
SRC:=main.c filesplitter.c utils.c handle_gadget.c split_gadget.c progressbar.c mpi_wrapper.c
INCL:=filesplitter.h utils.h handle_gadget.h gadget_defs.h split_gadget.h progressbar.h mpi_wrapper.h

OPTIONS:= # -DUSE_MPI
OPTIMIZE:=-O3 -march=native
CCFLAGS:= 

include common.mk
SRC:=$(addprefix src/, $(SRC))
INCL:=$(addprefix src/, $(INCL))
OBJS:=$(SRC:.c=.o)


all: $(TARGET) $(SRC) $(INCL) Makefile

src/%.o:src/%.c Makefile common.mk $(INCL)
	$(CC) $(OPTIONS) $(CCFLAGS) $(OPTIMIZE) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CLINK) $^ -o $@

.phony: clena celan celna clean
clena celan celna: clean
clean:
	$(RM) $(OBJS) $(TARGET)
