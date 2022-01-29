/* AGGIUNGERE DESCRIZIONE SERVER MANAGER THREAD */

#ifndef serv_manager_h
#define serv_manager_h

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <sys/select.h>

#include "err_cleanup.h"
#include "definitions.h"
#include "parser.h"
#include "conc_fifo.h"
#include "serv_worker.h"
#include "sc_cache.h"

#define PIPE_MSG_LEN sizeof(int)

sc_cache* server_cache;      // the actual cache structure used by the server
size_t server_byte_size;      // indicates the capacity of the server in terms of bytes of memory
size_t server_file_size;      // indicates the capacity of the server in terms of files in memory

unsigned long thread_pool_cap;      // indicates the maximum number of worker-threads the server can manage at the same time
char* sock_addr;       // will hold the server's main socket address
conc_queue* requests_queue;        // the queue worker-threads will use to dispatch client's requests
conc_queue* log_queue;          // the queue used to pass log-messages to the logger thread
pthread_t* worker_threads_arr;      // array of worker threads
pthread_t logger_thread;        // logger thread, handling statistics/information logging

unsigned short fd_pipe_read;    // will hold the read descriptor for the communication pipe between manager and workers
unsigned short fd_pipe_write;   // will hold the write descriptor for the communication pipe between manager and workers

volatile __sig_atomic_t soft_close;     // tells the threads to finish all the remaining work and shut down
volatile __sig_atomic_t hard_close;     // tells the threads to finish only the current request and shut down

int read_config(char* path);       // Reads the configuration file and extracts its information
int check_dir_path(char* path);     // Checks if the given path's format is correct

#endif // serv_manager_h