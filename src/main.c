#include <stdio.h>
#include <stdlib.h>

#include "split_gadget.h"
#include "mpi_wrapper.h"

void Printhelp(void);

int main(int argc, char **argv)
{
    char *input_filebase=NULL, *output_filebase=NULL;
    int noutfiles;
    const char argnames[][30]={"Input filebase","Output filebase","Number of output files", "Copy method"};
    int nargs=sizeof(argnames)/(sizeof(char)*30);

#ifdef USE_MPI
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &ThisTask);
	MPI_Comm_size(MPI_COMM_WORLD, &Ntasks);
#else
	Ntasks = 1;
	ThisTask = 0;
#endif

    /*---Read-arguments-----------------------------------*/
    if(argc< (nargs+1)) {
        Printhelp() ;
        fprintf(stderr,"\nFound: %d parameters\n ",argc-1);
        int i;
        for(i=1;i<argc;i++) {
            if(i <= nargs)
                fprintf(stderr,"\t\t %s = `%s' \n",argnames[i-1],argv[i]);
            else
                fprintf(stderr,"\t\t <> = `%s' \n",argv[i]);
        }
        if(i <= nargs) {
            fprintf(stderr,"\nMissing required parameters \n");
            for(i=argc;i<=nargs;i++)
                fprintf(stderr,"\t\t %s = `?'\n",argnames[i-1]);
        }
        return EXIT_FAILURE;
    }

    input_filebase=argv[1];
    output_filebase=argv[2];
    noutfiles=atoi(argv[3]);
	int copy_type = atoi(argv[4]);
	if(copy_type >= NUM_FCPY_OPT) {
	  fprintf(stderr,"Error: Invalid value = %d for copy method. Must be in range [0, %d) \n",
			  copy_type, NUM_FCPY_OPT);
	  Printhelp();
	  return EXIT_FAILURE;
	}
    
	if(ThisTask == 0) {
	  fprintf(stderr,"Running `%s' with the parameters \n",argv[0]);
	  fprintf(stderr,"\n\t\t -------------------------------------\n");
	  for(int i=1;i<argc;i++) {
        if(i <= nargs) {
		  fprintf(stderr,"\t\t %-10s = %s \n",argnames[i-1],argv[i]);
        }  else {
		  fprintf(stderr,"\t\t <> = `%s' \n",argv[i]);
        }
	  }
	  fprintf(stderr,"\t\t Ntasks = %d \n", Ntasks);
	  fprintf(stderr,"\t\t -------------------------------------\n");
	}

	file_copy_options copy_kind = copy_type;
    int status = split_gadget(input_filebase, output_filebase, noutfiles, copy_kind);
#ifdef USE_MPI
	if(status != EXIT_SUCCESS) {
	  MPI_Abort(MPI_COMM_WORLD, status);
	}
	MPI_Finalize();
#endif

    return status;
}

/*---Print-help-information---------------------------*/
void Printhelp(void)
{
    fprintf(stderr,"=========================================================================\n") ;
    fprintf(stderr,"   --- ./gadget_file_splitter input_filebase output_filebase noutputfiles\n") ;
    fprintf(stderr,"   --- Splits a multi-file Gadget snapshot file into so that all of the\n");
    fprintf(stderr,"       new chunks contain roughly the same number of particles\n");
    fprintf(stderr,"     * Input filebase           = Basename of the input files (full-path)\n") ;
    fprintf(stderr,"     * Output filebase          = Basename of the output files (full-path)\n");
    fprintf(stderr,"     * Number of output files   = Number of files to split into\n") ;
	fprintf(stderr,"     * Copy strategy            = (pread = %d, mmap + memcpy = %d, fread = %d\n",
			PRDWR, MEMCP, FRDWR) ;	
    fprintf(stderr,"=============================================================================\n") ;

}
