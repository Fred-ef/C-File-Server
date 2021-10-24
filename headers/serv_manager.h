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
#include "conc_fifo.h"

unsigned short thread_pool_cap;      // Indicates the maximum number of worker-threads the server can manage at the same time
char* sock_addr=NULL;       // will hold the server's main socket address
conc_queue* requests_queue;        // the queue worker-threads will use to dispatch client's requests TODO cambiare tipo
pthread_t* worker_threads_arr;      // array of worker threads
pthread_t cleaner_thread;       // cleaner thread, handling file closing and lock-unlocking, should a client crash
pthread_t logger_thread;        // logger thread, handling statistics/information logging

volatile __sig_atomic_t soft_close;     // communicates the threads to finish all the remaining work and shut down
volatile __sig_atomic_t hard_close;     // communicates the threads to finish only the current request and shut down

int read_config(char* path);       // Reads the configuration file and extracts its information
int check_dir_path(char* path);     // Checks if the given path's format is correct

#endif // serv_manager_h