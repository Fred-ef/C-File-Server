/* DESCRIZIONE SERVER WORKER */

#ifndef serv_worker_h
#define serv_worker_h

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "definitions.h"
#include "err_cleanup.h"

void* worker_func(void* arg);       // main thread function

#endif // serv_worker_h