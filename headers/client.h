/* SCRIVERE DESCRIZIONE - HEADER FILE PER IL CLIENT */

#ifndef client_h
#define client_h

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dirent.h>

#include "err_cleanup.h"
#include "definitions.h"
#include "parser.h"
#include "linkedlist.h"

char f_flag;      // used in order not to duplicate the -f command
char p_flag;      // used in order not to duplicate the -p command

byte D_flag;      // used in order  to couple -D with -w or -W
byte d_flag;      // used in order  to couple -d with -r or -R

byte t_flag;      // used in order to temporally distantiate consecutive requests

byte sleep_time;      // used to set a sleep between consecutive requests

byte conn_timeout;    // used to set a time-out to connection attempts
unsigned conn_delay;      // used to set a time margin between consecutive connection attempts

llist* open_files_list;


// definition of the output-printing function (only prints if p is flagged)
#define LOG_OUTPUT(fmt, ...) \
    if(p_flag) fprintf(stdout, fmt, ##__VA_ARGS__);

#endif // client_h