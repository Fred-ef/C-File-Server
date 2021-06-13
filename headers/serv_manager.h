/* AGGIUNGERE DESCRIZIONE SERVER MANAGER THREAD */

#ifndef serv_manager_h
#define serv_manager_h

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "err_cleanup.h"
#include "definitions.h"
#include "parser.h"

void read_config(char* path);       // Reads the configuration file and extracts its information

#endif // serv_manager_h