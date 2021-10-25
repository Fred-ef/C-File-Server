#include "serv_manager.h"

sc_cache* server_cache=NULL;      // the actual cache structure used by the server
unsigned server_byte_size=0;        // indicates the capacity of the server in terms of bytes of memory
unsigned server_file_size=0;        // indicates the capacity of the server in terms of files in memory

unsigned thread_pool_cap=0;     // Indicates the maximum number of worker-threads the server can manage at the same time
char* sock_addr=NULL;       // will hold the server's main socket address     TODO FREE ALLA FINE
pthread_t* worker_threads_arr=NULL;      // array of worker threads    TODO FREE ALLA FINE
pthread_t cleaner_thread;       // cleaner thread, handling file closing and lock-unlocking, should a client crash
pthread_t logger_thread;        // logger thread, handling statistics/information logging
conc_queue* requests_queue=NULL;    // the queue worker-threads will use to dispatch client's requests TODO cambiare tipo TODO FREE ALLA FINE

unsigned short fd_pipe_read;    // will hold the read descriptor for the communication pipe between manager and workers
unsigned short fd_pipe_write;   // will hold the write descriptor for the communication pipe between manager and workers

volatile __sig_atomic_t soft_close=0;     // communicates the threads to finish all the remaining work and shut down
volatile __sig_atomic_t hard_close=0;     // communicates the threads to finish only the current request and shut down

static int read_config(char*);     // parses the config file given, assigning its values to the variables specified
static int check_dir_path(char*);       // checks the given path, determining if it is a valid directory path


int main(int argc, char* argv[]) {

    // #################### COMMAND LINE CHECKS ####################

    if(argc<=1) {LOG_ERR(EINVAL, "config file missing"); goto cleanup_x;}      // immidiately terminates if no param is passed in input
    if((read_config(argv[1]))==ERR) {LOG_ERR(EINVAL, "cnfg err"); goto cleanup_x;}       // reads the configuration to determine the server's address


    // #################### DECLARATIONS ####################

    unsigned short i;       // for loop index
    short temperr;      // memorizes error codes in syscalls
    int int_buf;        // acts as a buffer for int-value-reads
    short fd_sk, fd_cl;        // file descriptor for the manager's sockets and for clients connection
    struct sockaddr_un sa;      // represents the manager's socket address

    fd_set curr_set, rd_set;      // primary and backup fd_sets
    unsigned short max_fd_active=0;  // will indicate the maximum fd reached in the accept call


    // #################### MEM ALLOCATION ####################

    server_cache=sc_cache_create(server_file_size, server_byte_size);   // creates the cache structure
    if(!server_cache) {LOG_ERR(errno, "manager: allocating server cache"); goto cleanup_x;}

    worker_threads_arr=(pthread_t*)malloc(thread_pool_cap*sizeof(pthread_t));   // allocates memory for all worker thread
    if(!worker_threads_arr) {LOG_ERR(errno, "manager: allocating worker threads"); goto cleanup_x;}

    requests_queue=conc_fifo_create(NULL);      // initializes the concurrent queue
    if(!requests_queue) {LOG_ERR(errno, "manager: creating request queue"); goto cleanup_x;}

    for(i=0; i<thread_pool_cap; i++) {
        if((temperr=pthread_create(&(worker_threads_arr[i]), NULL, worker_func, NULL))==ERR) {
            LOG_ERR(temperr, "manager: creating worker threads");
            goto cleanup_x;
        }
    }

    /*
    if((temperr=pthread_create(&(cleaner_thread), NULL, serv_clean, NULL))==ERR) {
        LOG_ERR(temperr, "manager: creating cleaner thread");
        goto cleanup_x;
    }

    if((temperr=pthread_create(&(logger_thread), NULL, serv_log, NULL))==ERR) {
        LOG_ERR(temperr, "manager: creating logger thread");
        goto cleanup_x;
    }
    */


    // #################### PIPE SETUP ####################

    int pfd[2];     // creating pipe args
    if((temperr=pipe(pfd))==ERR) {LOG_ERR(errno, "manager: creating pipe"); goto cleanup_x;}
    fd_pipe_read=pfd[0];    // setting pipe's read fd
    fd_pipe_write=pfd[1];   // setting pipe's write fd


    // #################### SERVER SETUP ####################

    strcpy(sa.sun_path, sock_addr);       // writes the sock address in the socket adress structure
    sa.sun_family = AF_UNIX;        // specifying the socket family

    if((fd_sk=socket(AF_UNIX, SOCK_STREAM, 0))==ERR) {      // client socket creation
        LOG_ERR(errno, "manager: creating client socket");
        goto cleanup_x;
    }

    if((temperr=bind(fd_sk, (struct sockaddr*) &sa, sizeof(sa)))==ERR) {
        LOG_ERR(errno, "manager: binding address to socket");
        goto cleanup_x;
    }

    if((temperr=listen(fd_sk, SOMAXCONN))==ERR) {
        LOG_ERR(errno, "manager: listening on the socket");
        goto cleanup_x;
    }

    FD_ZERO(&curr_set);                 // zeroing the curr fd set
    FD_SET(fd_sk, &curr_set);           // setting the socket's fd in the curr fd set
    FD_SET(fd_pipe_read, &curr_set);    // setting the pipe's read fd in the curr fd set
    if(fd_sk>fd_pipe_read) max_fd_active=fd_sk;     // updating the number of fd currently active
    else max_fd_active=fd_pipe_read;    // NOTE: fd_sk cannot be equal to fd_pipe, so one else is enough


    // #################### SERVER MAIN LOOP ####################

    while(!soft_close && !hard_close) {
        rd_set=curr_set;    // re-setting the ready fd set

        if((temperr=select(max_fd_active+1, &rd_set, NULL, NULL, NULL))==ERR) {
            LOG_ERR(errno, "manager: error in select");
            goto cleanup_x;
        }

        for(i=0; i<=max_fd_active; i++) {   // begin checking for fds
            
            if(FD_ISSET(i, &rd_set)) {  // found a ready fd
                  // start checking for which fd is ready

                  if(i==fd_sk) {    // connection request
                      if((fd_cl=accept(fd_sk, NULL, 0))==ERR)
                        {LOG_ERR(errno, "manager: error while accepting client connection"); goto cleanup_x;}
                      FD_SET(fd_cl, &curr_set);     // adding client's fd in the curr fd set
                      if(fd_cl>max_fd_active) max_fd_active=fd_cl;
                  }

                  else if(i==fd_pipe_read) {    // worker-manager communication
                    if((temperr=readn(fd_pipe_read, &int_buf, PIPE_MSG_LEN))==ERR) {
                        LOG_ERR(EPIPE, "manager: reading from pipe");
                        goto cleanup_x;
                    }
                    // if manager got an invalid read
                    if(!int_buf) {LOG_ERR(EPIPE, "manager: elaborating pipe value"); goto cleanup_x;}
                    // else elaborate the result
                    if(int_buf<0) {     // the client disconnected
                        int_buf*=-1;    // inverting sign to get the actual fd
                        FD_CLR(int_buf, &curr_set);     // removing the fd from the curr fd set
                        close(int_buf);     // closing communication
                    }
                    // client request served
                    else FD_SET(int_buf, &curr_set);    // reinserting the client fd in the curr fd set
                  }

                  else {    // generic request by a connected client
                      int* client_fd=malloc(sizeof(int));
                      *client_fd=i;

                      if((temperr=pthread_mutex_lock(&(requests_queue->queue_mtx)))==ERR) {
                          LOG_ERR(temperr, "manager: locking request queue"); goto cleanup_x;
                      }
                      if((temperr=conc_fifo_push(requests_queue, (void*)client_fd))==ERR) {
                          LOG_ERR(errno, "manager: pushing request into queue"); goto cleanup_x;
                      }
                      if((temperr=pthread_cond_signal(&(requests_queue->queue_cv)))==ERR) {
                          LOG_ERR(temperr, "manager: signaling request to workers"); goto cleanup_x;
                      }
                      if((temperr=pthread_mutex_unlock(&(requests_queue->queue_mtx)))==ERR) {
                          LOG_ERR(temperr, "manager: unlocking request queue"); goto cleanup_x;
                      }
                      FD_CLR(i, &curr_set);     // remove client's fd from curr fd set until his request is served
                  }
            }
        }
    }


    // #################### LOG SECTION ####################



    // #################### SERVER CLOSURE ####################

    for(i=0; i<thread_pool_cap; i++) {      // joining worker threads
        if((temperr=pthread_join(worker_threads_arr[i], NULL))==ERR) {
            LOG_ERR(temperr, "manager: joining workers");
            goto cleanup_x;
        }
    }

    if((temperr=pthread_join(cleaner_thread, NULL))==ERR) {     // joining cleaner thread
        LOG_ERR(temperr, "manager: joining cleaner");
        goto cleanup_x;
    }

    if((temperr=pthread_join(logger_thread, NULL))==ERR) {      // joining logger thread
        LOG_ERR(temperr, "manager: joining logger");
        goto cleanup_x;
    }

    close(fd_pipe_read);    // closing read pipe
    close(fd_pipe_write);   // closing write pipe
    close(fd_sk);       // closing the connection to clients

    if(server_cache) free(server_cache);
    if(sock_addr) free(sock_addr);
    if(worker_threads_arr) free(worker_threads_arr);
    if(requests_queue) fifo_dealloc_full(requests_queue);
    exit(EXIT_SUCCESS);         // Successful termination

// #################### CLEANUP SECTION ####################
cleanup_x:
    if(server_cache) free(server_cache);
    if(sock_addr) free(sock_addr);
    if(worker_threads_arr) free(worker_threads_arr);
    if(requests_queue) fifo_dealloc_full(requests_queue);
    exit(EXIT_FAILURE);         // Erroneus termination
}



/* #################################################################################################### */
/* ######################################### HELPER FUNCTIONS ######################################### */
/* #################################################################################################### */


static int read_config(char* path) {
    // TODO gestire eventuali errori nella funzione di parsing
    if((parse(path, "thread_pool_cap", &thread_pool_cap))==ERR) return ERR;
    if((parse_s(path, "sock_addr", &sock_addr))==ERR) return ERR;

    return SUCCESS;
}


static int check_dir_path(char* path) {
    int i;      // for loop index
    char temp='a';      // temp var

    for(i=0; temp!='\0'; i++) {     // iterating until the end of the string
        temp=path[i];
    } i--;  // decrementing index at the end (it's the index of the null char)

    if(path[i]!='/') return ERR;

    return SUCCESS;
}