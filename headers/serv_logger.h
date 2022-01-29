/* AGGIUNGERE DESCRIZIONE SERVER MANAGER THREAD */

#ifndef serv_logger_h
#define serv_logger_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "serv_manager.h"

void* logger_func(void* arg);   // main thread function

#endif // serv_logger_h