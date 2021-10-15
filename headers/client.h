/* SCRIVERE DESCRIZIONE - HEADER FILE PER IL CLIENT */

#ifndef client_h
#define client_h

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
// #include <string.h>

#include "err_cleanup.h"
#include "definitions.h"
#include "parser.h"
#include "client_server_comm.h"

#define MAX_ARG_NUM 10
#define MAX_ARG_LEN 128

char f_flag=0;      // used in order not to duplicate the -f command
char h_flag=0;      // used in order not to duplicate the -h command
char p_flag=0;      // used in order not to duplicate the -p command

char D_flag=0;      // used in order  to couple -D with -w or -W
char d_flag=0;      // used in order  to couple -d with -r or -R

char t_flag=0;      // used in order to temporally distantiate consecutive requests


byte conn_timeout=0;    // used to set a time-out to connection attempts
byte conn_delay=0;      // used to set a time margin between consecutive connection attempts

static int parse_command(char**);       // parses the command line, executing requests one by one

#endif // client_h