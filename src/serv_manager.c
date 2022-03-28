#include "serv_manager.h"

sc_cache* server_cache=NULL;      // the actual cache structure used by the server
size_t server_byte_size=0;        // indicates the capacity of the server in terms of bytes of memory
size_t server_file_size=0;        // indicates the capacity of the server in terms of files in memory

unsigned long thread_pool_cap=0;     // Indicates the maximum number of worker-threads the server can manage at the same time
char* sock_addr=NULL;       // will hold the server's main socket address
char* log_file_path=NULL;        // will hold the logfile's pathname
pthread_t* worker_threads_arr=NULL;      // array of worker threads
pthread_t logger_thread;        // logger thread, handling statistics/information logging
conc_queue* requests_queue=NULL;    // the queue worker-threads will use to dispatch client's requests
conc_queue* log_queue=NULL;     // the queue used to pass log-messages to the logger thread
conc_queue** open_files=NULL;    // holds a description of all files opened by every possible client

unsigned short fd_pipe_read;    // will hold the read descriptor for the communication pipe between manager and workers
unsigned short fd_pipe_write;   // will hold the write descriptor for the communication pipe between manager and workers

volatile __sig_atomic_t soft_close=0;     // communicates the threads to finish all the remaining work and shut down
volatile __sig_atomic_t hard_close=0;     // communicates the threads to finish only the current request and shut down

int read_config(char*);     // parses the config file given, assigning its values to the variables specified
int check_dir_path(char*);       // checks the given path, determining if it is a valid directory path

static void sigint_handler(int);    // handler for SIGINT
static void sigquit_handler(int);   // handler for SIGQUIT
static void sighup_handler(int);    // handler for SIGHUP


int main(int argc, char* argv[]) {

    // #################### COMMAND LINE CHECKS ####################

    if(argc<=1) {LOG_ERR(EINVAL, "config file missing"); goto cleanup_manager;}      // immidiately terminates if no param is passed in input
    if((read_config(argv[1]))==ERR) {LOG_ERR(EINVAL, "cnfg err"); goto cleanup_manager;}       // reads the configuration to determine the server's address


    // #################### DECLARATIONS ####################

    unsigned i;       // for loop index
    short temperr;      // memorizes error codes in syscalls
    int* int_buf;        // acts as a buffer for int-value-reads
    short fd_sk, fd_cl;        // file descriptor for the manager's sockets and for clients connection
    struct sockaddr_un sa;      // represents the manager's socket address

    fd_set curr_set, rd_set;      // primary and backup fd_sets
    unsigned short max_fd_active=0;  // will indicate the maximum fd reached in the accept call
    unsigned short max_def_fd=0;    // will indicate the maximum fd belonging to the server (not client-related)


    // #################### SIGNAL HANDLING ####################

    struct sigaction sigint_act;
    struct sigaction sigquit_act;
    struct sigaction sighup_act;
    struct sigaction sigpipe_act;

    memset(&sigint_act, 0, sizeof(struct sigaction));
    memset(&sigquit_act, 0, sizeof(struct sigaction));
    memset(&sighup_act, 0, sizeof(struct sigaction));
    memset(&sigpipe_act, 0, sizeof(struct sigaction));

    sigint_act.sa_handler=sigint_handler;
    if((temperr=sigaction(SIGINT, &sigint_act, NULL))==ERR) {
        LOG_ERR(errno, "MANAGER: assigning SIGINT handler");
        goto cleanup_manager;
    }

    sigquit_act.sa_handler=sigquit_handler;
    if((temperr=sigaction(SIGQUIT, &sigquit_act, NULL))==ERR) {
        LOG_ERR(errno, "MANAGER: assigning SIGQUIT handler");
        goto cleanup_manager;
    }

    sighup_act.sa_handler=sighup_handler;
    if((temperr=sigaction(SIGHUP, &sighup_act, NULL))==ERR) {
        LOG_ERR(errno, "MANAGER: assigning SIGHUP handler");
        goto cleanup_manager;
    }

    sigpipe_act.sa_handler=SIG_IGN;


    // #################### MEM ALLOCATION ####################

    server_cache=sc_cache_create(server_file_size, server_byte_size);   // creates the cache structure
    if(!server_cache) {LOG_ERR(errno, "manager: allocating server cache"); goto cleanup_manager;}

    worker_threads_arr=(pthread_t*)malloc(thread_pool_cap*sizeof(pthread_t));   // allocates memory for all worker thread
    if(!worker_threads_arr) {LOG_ERR(errno, "manager: allocating worker threads"); goto cleanup_manager;}

    requests_queue=conc_fifo_create(NULL);      // initializes the concurrent queue
    if(!requests_queue) {LOG_ERR(errno, "manager: creating request queue"); goto cleanup_manager;}

    log_queue=conc_fifo_create(NULL);      // initializes the concurrent queue
    if(!log_queue) {LOG_ERR(errno, "manager: creating log queue"); goto cleanup_manager;}

    for(i=0; i<thread_pool_cap; i++) {
        int* id=(int*)malloc(sizeof(int));
        if(!id) {LOG_ERR(errno, "manager: creating worker threads arg"); goto cleanup_manager;}
        *id=i;
        if((temperr=pthread_create(&(worker_threads_arr[i]), NULL, worker_func, (void*)id))==ERR) {
            LOG_ERR(temperr, "manager: creating worker threads");
            goto cleanup_manager;
        }
    }

    if((temperr=pthread_create(&(logger_thread), NULL, logger_func, NULL))==ERR) {
        LOG_ERR(temperr, "manager: creating logger thread");
        goto cleanup_manager;
    }

   int_buf=(int*)malloc(sizeof(int));   // allocating memory for the buffer used for int value reads
   if(!int_buf) {LOG_ERR(errno, "manager: allocating int buffer"); goto cleanup_manager;}


   open_files=(conc_queue**)calloc(SOMAXCONN, sizeof(conc_queue*)); // allocating memory for the queue tracking open files
   if(!open_files) {LOG_ERR(errno, "manager: allocating open files queue"); goto cleanup_manager;}

   // allocating open files queues
   for(i=0; i<SOMAXCONN; i++) {
       open_files[i]=conc_fifo_create(NULL);
       if(!(open_files[i])) {
           LOG_ERR(errno, "manager: allocating open files queue entries");
           goto cleanup_manager;
       }
   }


    // #################### PIPE SETUP ####################

    int pfd[2];     // creating pipe args
    if((temperr=pipe(pfd))==ERR) {LOG_ERR(errno, "manager: creating pipe"); goto cleanup_manager;}
    fd_pipe_read=pfd[0];    // setting pipe's read fd
    fd_pipe_write=pfd[1];   // setting pipe's write fd


    // #################### SERVER SETUP ####################

    strcpy(sa.sun_path, sock_addr);       // writes the sock address in the socket adress structure
    sa.sun_family = AF_UNIX;        // specifying the socket family

    if((fd_sk=socket(AF_UNIX, SOCK_STREAM, 0))==ERR) {      // client socket creation
        LOG_ERR(errno, "manager: creating client socket");
        goto cleanup_manager;
    }

    if((temperr=bind(fd_sk, (struct sockaddr*) &sa, sizeof(sa)))==ERR) {
        LOG_ERR(errno, "manager: binding address to socket");
        goto cleanup_manager;
    }

    if((temperr=listen(fd_sk, SOMAXCONN))==ERR) {
        LOG_ERR(errno, "manager: listening on the socket");
        goto cleanup_manager;
    }

    FD_ZERO(&curr_set);                 // zeroing the curr fd set
    FD_SET(fd_sk, &curr_set);           // setting the socket's fd in the curr fd set
    FD_SET(fd_pipe_read, &curr_set);    // setting the pipe's read fd in the curr fd set
    if(fd_sk>fd_pipe_read) max_fd_active=fd_sk;     // updating the number of fd currently active
    else max_fd_active=fd_pipe_read;    // NOTE: fd_sk cannot be equal to fd_pipe, so one else is enough
    max_def_fd=max_fd_active;   // updating the max default (server related) fd


    // #################### SERVER MAIN LOOP ####################

    while(!hard_close) {
        rd_set=curr_set;    // re-setting the ready fd set

        if(soft_close) {    // if the server should gently terminate...
            if(max_fd_active==max_def_fd) break;    // no client has yet connected, terminate

            int close_decision=TRUE;
            // if at least one client is still active, don't close it
            for(i=max_def_fd+1; i<=max_fd_active; i++) {
                if((fcntl(i, F_GETFD))!=ERR) close_decision=FALSE;
            } 

            if(close_decision) break;
        }

        if((temperr=select(max_fd_active+1, &rd_set, NULL, NULL, NULL))==ERR) {
            if(errno==EINTR) continue;  // catched a signal
            LOG_ERR(errno, "manager: error in select");
            goto cleanup_manager;
        }

        for(i=0; i<=max_fd_active; i++) {   // begin checking for fds
            
            if(FD_ISSET(i, &rd_set)) {  // found a ready fd
                // start checking for which fd is ready

                if(i==fd_sk) {    // connection request
                    if((fd_cl=accept(fd_sk, NULL, 0))==ERR)
                        {LOG_ERR(errno, "manager: error while accepting client connection"); goto cleanup_manager;}
                    FD_SET(fd_cl, &curr_set);     // adding client's fd in the curr fd set
                    if(fd_cl>max_fd_active) max_fd_active=fd_cl;
                }

                else if(i==fd_pipe_read) {    // worker-manager communication
                    // reading the message
                    if((temperr=readn(fd_pipe_read, int_buf, PIPE_MSG_LEN))==ERR) {
                        LOG_ERR(EPIPE, "manager: reading from pipe");
                        goto cleanup_manager;
                    }
                    // if manager got an invalid read
                    if(!int_buf || !(*int_buf)) {LOG_ERR(EPIPE, "manager: elaborating pipe value"); goto cleanup_manager;}
                    // else elaborate the result
                    if(*int_buf<0) {     // the client disconnected
                        (*int_buf)*=-1;    // inverting sign to get the actual fd
                        if((usr_close_all(server_cache, int_buf))==ERR)
                        {LOG_ERR(errno, "MANAGER: error closing client files"); goto cleanup_manager;}
                        FD_CLR(*int_buf, &curr_set);     // removing the fd from the curr fd set
                        close(*int_buf);     // closing communication
                    }
                    // client request served
                    else FD_SET(*int_buf, &curr_set);   // reinserting the client fd in the curr fd set
                }

                else {    // generic request by a connected client
                    LOG_OPERATION("##########\nQueued client %u's request\n##########\n\n", i);
                    int* client_fd=malloc(sizeof(int));
                    if(!client_fd) {LOG_ERR(errno, "manager: preparing client fd"); goto cleanup_manager;}
                    *client_fd=i;

                    if((temperr=pthread_mutex_lock(&(requests_queue->queue_mtx)))==ERR) {
                        LOG_ERR(temperr, "manager: locking request queue"); goto cleanup_manager;
                    }
                    if((temperr=conc_fifo_push(requests_queue, (void*)client_fd))==ERR) {
                        LOG_ERR(errno, "manager: pushing request into queue"); goto cleanup_manager;
                    }
                    if((temperr=pthread_cond_signal(&(requests_queue->queue_cv)))==ERR) {
                        LOG_ERR(temperr, "manager: signaling request to workers"); goto cleanup_manager;
                    }
                    if((temperr=pthread_mutex_unlock(&(requests_queue->queue_mtx)))==ERR) {
                        LOG_ERR(temperr, "manager: unlocking request queue"); goto cleanup_manager;
                    }
                    FD_CLR(i, &curr_set);     // remove client's fd from curr fd set until his request is served
                  }
            }
        }
    }

    hard_close=1;   // notifying all threads to stop working


    // #################### LOGGING SECTION ####################

    int logfile_fd;
    if((logfile_fd=open(log_file_path, O_WRONLY | O_APPEND | O_CREAT, 0777))==ERR) {
        LOG_ERR(errno, "logger: opening log file");
        exit(EXIT_FAILURE);
    }

    dprintf(logfile_fd, "##########\nMANAGER: max cache file size: %lu files\n##########\n\n", max_file_number_reached);
    dprintf(logfile_fd, "##########\nMANAGER: max cache byte size: %lu bytes\n##########\n\n", max_byte_size_reached);
    dprintf(logfile_fd, "##########\nMANAGER: max clients connected at the same time: %u clients\n##########\n\n", (max_fd_active-max_def_fd));
    dprintf(logfile_fd, "##########\nMANAGER: total number of expelled files: %lu files\n##########\n\n", subst_file_num);

    if((close(logfile_fd))==ERR) {
        LOG_ERR(errno, "logger: closing log file");
        exit(EXIT_FAILURE);
    }


    // #################### THREADS TERMINATION ####################

    // awakening worker threads
    if((temperr=pthread_mutex_lock(&(requests_queue->queue_mtx)))==ERR) {
        LOG_ERR(temperr, "manager: locking request queue"); goto cleanup_manager;
    }
    if((temperr=pthread_cond_broadcast(&(requests_queue->queue_cv)))==ERR) {
        LOG_ERR(temperr, "manager: signaling request to workers"); goto cleanup_manager;
    }
    if((temperr=pthread_mutex_unlock(&(requests_queue->queue_mtx)))==ERR) {
        LOG_ERR(temperr, "manager: unlocking request queue"); goto cleanup_manager;
    }
    // joining worker threads
    for(i=0; i<thread_pool_cap; i++) {
        if((temperr=pthread_join(worker_threads_arr[i], NULL))==ERR) {
            LOG_ERR(temperr, "manager: joining workers");
            goto cleanup_manager;
        }
    }

    
    // awakening logger thread
    if((temperr=pthread_mutex_lock(&(log_queue->queue_mtx)))==ERR) {
        LOG_ERR(temperr, "logging: locking log queue"); return ERR;
    }
    if((temperr=pthread_cond_signal(&(log_queue->queue_cv)))==ERR) {
        LOG_ERR(temperr, "logging: signaling log to logger"); return ERR;
    }
    if((temperr=pthread_mutex_unlock(&(log_queue->queue_mtx)))==ERR) {
        LOG_ERR(temperr, "logging: unlocking log queue"); return ERR;
    }
    // joining logger thread
    if((temperr=pthread_join(logger_thread, NULL))==ERR) {
        LOG_ERR(temperr, "manager: joining logger");
        goto cleanup_manager;
    }



    // #################### SERVER CLOSURE ####################

    close(fd_pipe_read);    // closing read pipe
    close(fd_pipe_write);   // closing write pipe
    close(fd_sk);       // closing the connection to clients

    // deallocating open files queue
    for(i=0; i<SOMAXCONN; i++) {
        if((temperr=fifo_dealloc_full(open_files[i]))==ERR) {
            LOG_ERR(errno, "manager: freeing open files queue entries");
            goto cleanup_manager;
        }
    }

    if((temperr=sc_cache_clean(server_cache))==ERR) {
        LOG_ERR(errno, "manager: wiping cache");
        goto cleanup_manager;
    }

    if(sock_addr) free(sock_addr);
    if(log_file_path) free(log_file_path);
    if(worker_threads_arr) free(worker_threads_arr);
    if(int_buf) free(int_buf);
    if(requests_queue) fifo_dealloc_full(requests_queue);
    if(log_queue) fifo_dealloc_full(log_queue);
    if(open_files) free(open_files);
    exit(EXIT_SUCCESS);         // Successful termination

// #################### CLEANUP SECTION ####################
cleanup_manager:
    if(server_cache) free(server_cache);
    if(sock_addr) free(sock_addr);
    if(log_file_path) free(log_file_path);
    if(worker_threads_arr) free(worker_threads_arr);
    if(int_buf) free(int_buf);
    if(requests_queue) fifo_dealloc_full(requests_queue);
    if(log_queue) fifo_dealloc_full(log_queue);
    if(open_files) free(open_files);
    exit(EXIT_FAILURE);         // Erroneus termination
}



/* #################################################################################################### */
/* ######################################### HELPER FUNCTIONS ######################################### */
/* #################################################################################################### */


int read_config(char* path) {
    if((parse(path, "thread_pool_cap", &thread_pool_cap))==ERR) return ERR;
    if((parse_s(path, "sock_addr", &sock_addr))==ERR) return ERR;
    if((parse_s(path, "log_file_path", &log_file_path))==ERR) return ERR;
    if((parse(path, "server_byte_size", &server_byte_size))==ERR) return ERR;
    if((parse(path, "server_file_size", &server_file_size))==ERR) return ERR;

    return SUCCESS;
}


int check_dir_path(char* path) {
    int i;      // for loop index
    char temp='a';      // temp var

    for(i=0; temp!='\0'; i++) {     // iterating until the end of the string
        temp=path[i];
    } i--;  // decrementing index at the end (it's the index of the null char)

    if(path[i]!='/') return ERR;

    return SUCCESS;
}


// ########## SIGNAL HANDLERS ##########

static void sigint_handler(int signum) {
    hard_close=1;
    return;
}

static void sigquit_handler(int signum) {
    hard_close=1;
    return;
}

static void sighup_handler(int signum) {
    soft_close=1;
    return;
}