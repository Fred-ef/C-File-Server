/*  This header file contains project-wise definitions of macros, types, values etc */

#ifndef definitions_h     // Multiple include protection
#define definitions_h

#define CONFIG_PATH "./config.ini" /* Path of the config.ini file */
#define TRUE 1     // Definition for the boolean value "true"
#define FALSE 0     // Definition for the boolean value "false"
#define MS_TO_NS_CONV 1000000     // Conversion rate of milliseconds to nanoseconds and viceversa
#define MS_TO_S_CONV 1000     // Conversion rate of milliseconds to seconds and viceversa
#define S_TO_NS_CONV 1000000000   // Conversion rate of seconds to nanoseconds and viceversa
#define UNIX_PATH_MAX 108       // Max dimension of a Unix socket address

#define MS_TO_NS(a) a*MS_TO_NS_CONV
#define NS_TO_MS(a) a/MS_TO_NS_CONV
#define S_TO_MS(a) a*MS_TO_S_CONV
#define MS_TO_S(a) a/MS_TO_S_CONV
#define S_TO_NS(a) a*S_TO_NS_CONV

typedef unsigned char bool;     // Type definition for the boolean type
typedef struct timespec stimespec;      // Type definition for the timespectype

#endif // definitions_h