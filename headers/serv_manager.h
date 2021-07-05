/* AGGIUNGERE DESCRIZIONE SERVER MANAGER THREAD */

#ifndef serv_manager_h
#define serv_manager_h

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

#include "err_cleanup.h"
#include "definitions.h"
#include "parser.h"

unsigned short thread_pool_cap;      // Indicates the maximum number of worker-threads the server can manage at the same time
char* sock_addr=NULL;       // will hold the server's main socket address
void requests_queue;        // the queue worker-threads will use to dispatch client's requests TODO cambiare tipo
pthread_mutex_t requests_queue_mtx;     // Serves as a mutual exclusion mutex for worker-threads to access the requests queue

int read_config(char* path);       // Reads the configuration file and extracts its information
int check_dir_path(char* path);     // Checks if the given path's format is correct

#endif // serv_manager_h