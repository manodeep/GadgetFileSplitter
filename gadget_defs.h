#pragma once

#include <stdint.h>
#define _FILE_OFFSET_BITS 64

struct io_header
{
    int32_t npart[6];                        /*!< number of particles of each type in this file */
    double mass[6];                      /*!< mass of particles of each type. If 0, then the masses are explicitly
                                           stored in the mass-block of the snapshot file, otherwise they are omitted */
    double time;                         /*!< time of snapshot file */
    double redshift;                     /*!< redshift of snapshot file */
    int32_t flag_sfr;                        /*!< flags whether the simulation was including star formation */
    int32_t flag_feedback;                   /*!< flags whether feedback was included (obsolete) */
    uint32_t npartTotal[6];          /*!< total number of particles of each type in this snapshot. This can be
                                       different from npart if one is dealing with a multi-file snapshot. */
    int32_t flag_cooling;                    /*!< flags whether cooling was included  */
    int32_t num_files;                       /*!< number of files in multi-file snapshot */
    double BoxSize;                      /*!< box-size of simulation in case periodic boundaries were used */
    double Omega0;                       /*!< matter density in units of critical density */
    double OmegaLambda;                  /*!< cosmological constant parameter */
    double HubbleParam;                  /*!< Hubble parameter in units of 100 km/sec/Mpc */
    int32_t flag_stellarage;                 /*!< flags whether the file contains formation times of star particles */
    int flag_metals;                     /*!< flags whether the file contains metallicity values for gas and star particles */
    uint32_t npartTotalHighWord[6];  /*!< High word of the total number of particles of each type */
    int32_t  flag_entropy_instead_u;         /*!< flags that IC-file contains entropy instead of u */
    char fill[60];                   /*!< fills to 256 Bytes */
};

struct file_mapping{
    int32_t *input_file_id;/* the input file id -> corresponds to the rank of the CPU that wrote out this snapshot/IC file */
    int32_t *input_file_start_particle;/* which particle to start outputting from (inclusive)*/
    int32_t *input_file_end_particle;/* which particle to end outputting at (excluse). Loops should be of the form */
    /* Loops to output the particles should be of this form  for(i=input_file_start_particle[k];i<input_file_end_particle[k];i++)*/ 
    int32_t *output_file_nwritten;/*For multiple input files, contains the cumulative sum of the particles written so far (must be <= numpart) */
    int64_t numpart;/* number of particles in this output file*/
    int64_t numfiles;//I could declare this as int but there would be 4 bytes of padding 
};

typedef enum {
    POS = 0,
    VEL = 1,
    ID = 2,
    NUM_FIELDS
}field_type;
    
    
