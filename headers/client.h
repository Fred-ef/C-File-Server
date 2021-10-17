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

char f_flag=0;      // used in order not to duplicate the -f command
char p_flag=0;      // used in order not to duplicate the -p command

byte D_flag=0;      // used in order  to couple -D with -w or -W
byte d_flag=0;      // used in order  to couple -d with -r or -R

byte t_flag=0;      // used in order to temporally distantiate consecutive requests

byte sleep_time=0;      // used to set a sleep between consecutive requests

byte conn_timeout=10;    // used to set a time-out to connection attempts
byte conn_delay=500;      // used to set a time margin between consecutive connection attempts

char* save_dir=NULL;    // used to specify the folder in which to save files retrieved from the file-server
char* miss_dir=NULL;    // used to specify the folder in which to save files discarded by the file-server

#endif // client_h