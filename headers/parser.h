/*  This code implements a small parser that reads from an input file containing certain value
    assignments (in this case, a configuration file config.ini), saving the parameter searched
    into a variable, casted to the correct type */

#ifndef parser_h     // Multiple include protection
#define parser_h

#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "part_rw_sol.h"     // Including implementation of readn and writen functions to address partial reads/writes
#include "definitions.h"     // Including definitions for ERR and SUCCESS values and for the path of the config.ini file
#include "err_cleanup.h"     // Including functions handling errors and memory management

#define RPERM O_RDONLY
#define WPERM O_WONLY
#define RWPERM O_RDWR

typedef struct stat sstat;     // Redefinition of the stat struct, used to retrieve information about a certain file

int parse(char*, char*, unsigned long*);     // Takes as input the parameter to search, the file in which the parameter is to be searched and the variable in which to save its value
int parse_s(char*, char*, char**);     // Takes as input the string parameter to search, the file in which the parameter is to be searched and the string in which to save its value

#endif     // parser_h