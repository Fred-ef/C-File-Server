/*  This file contains definitions used throughout the whole project for
    error-handling and clean-up purposes */

#ifndef err_cleanup_h
#define err_cleanup_h

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>


#define ERR (-1)     // Definition of the error value
#define MEMERR (-2)     // Definition of a value indicating a memory management error
#define SUCCESS (0)     // Definition of the success value

#define DEBUG
// #define MTX_DEBUG

#ifdef DEBUG     // Definition of a printing debug function
  #define LOG_DEBUG(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
/*
#else
  #define LOG_DEBUG(fmt, ...)
*/
#endif

// Definition an error-management printing function
#define LOG_ERR(err_code, err_desc) \
  errno=err_code; \
  fprintf(stderr, "%s: %s\n", err_desc, strerror(err_code));

#ifdef MTX_DEBUG      // Definition of a printing debug function for mutexes
  #define LOG_MTX(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
/*
#else
  #define LOG_MTX(fmt, ...)
*/
#endif

// The following functions taking care of mem-deallocation tasks take different parameter based on their focus
void cleanup_f(int, ...);     // Function handling the closure of a File Descriptor and the freeing of a non-definite number of pointers (terminating with a NULL pointer)
void cleanup_pointers(void*, ...);     // Function handling the freeing of a non-definite (and non-zero) number of pointers (terminating with a NULL pointer)

#endif // err_cleanup_h