/* SCRIVERE DESCRIZIONE - HEADER FILE PER IL CLIENT */

#ifndef client_h
#define client_h

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "err_cleanup.h"
#include "definitions.h"
#include "parser.h"

void read_config(char* path);       // Reads the configuration file and extracts its information

#endif // client_h