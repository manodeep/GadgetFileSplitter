#pragma once

#include <stdint.h>

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
