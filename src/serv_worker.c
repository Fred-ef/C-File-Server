#include "serv_worker.h"

// thread main
void* worker_func(void* arg) {

    int i;  // for loop index
    int temperr;    // used for error handling
    int* int_buf=NULL;   // serves as a buffer for int values
    int client_fd;     // will hold the fd of the user that sent the request
    int op;     // will hold the operation code representing the current request

    while(!hard_close) {

        if((temperr=pthread_mutex_lock(&(requests_queue->queue_mtx)))==ERR)
        {LOG_ERR(temperr, "worker: locking req queue"); exit(EXIT_FAILURE);}

        while(!(int_buf=(int*)conc_fifo_pop(requests_queue))) {  // checking fo new requests
            if(errno) {LOG_ERR(errno, "worker: reading from req queue"); exit(EXIT_FAILURE);}

            if((temperr=pthread_cond_wait(&(requests_queue->queue_cv), &(requests_queue->queue_mtx)))==ERR)
            {LOG_ERR(temperr, "worker: waiting on req queue"); exit(EXIT_FAILURE);}
        }

        if((temperr=pthread_mutex_unlock(&(requests_queue->queue_mtx)))==ERR)
        {LOG_ERR(temperr, "worker: unlocking req queue"); exit(EXIT_FAILURE);}

        client_fd=*int_buf;    // preparing to serve user's request
        free(int_buf);

        if((temperr=readn(client_fd, int_buf, PIPE_MSG_LEN))==ERR)     // getting the request into int_buf
        {LOG_ERR(EPIPE, "worker: reading client operation"); exit(EXIT_FAILURE);}

        op=*int_buf;    // reading the operation code
        free(int_buf);

        if(op==EOF) {   // the client close the connection
            int_buf=&client_fd;
            client_fd*=-1; // tells the manager to close the connection with this client
            // TODO assiurarsi che funzioni
            if((temperr=writen(fd_pipe_write, (void*)int_buf, PIPE_MSG_LEN))==ERR)
            {LOG_ERR(EPIPE, "worker: writing to the pipe");}
        }
        else if(op==OPEN_F) {
            if((temperr=worker_file_open(client_fd))==ERR)
            {LOG_ERR(errno, "worker: open file error"); exit(EXIT_FAILURE);}

            *int_buf=client_fd;    // tells the manager the operation is ocmplete
            if((temperr=writen(fd_pipe_write, (void*)int_buf, PIPE_MSG_LEN))==ERR)
            {LOG_ERR(EPIPE, "worker: writing to the pipe");}
        }
        else if(op==READ_F) {

        }
        else if(op==READ_N_F) {

        }
        else if(op==WRITE_F) {

        }
        else if(op==WRITE_F_APP) {

        }
        else if(op==LOCK_F) {

        }
        else if(op==UNLOCK_F) {

        }
        else if(op==RM_F) {

        }
        else if(op==CLOSE_F) {

        }
    }

    if(int_buf) free(int_buf);
    pthread_exit((void*)SUCCESS);
}


// ########## HELPER FUNCTIONS ##########

static int worker_file_open(int client_fd) {
    int temperr;
    int* int_buf=(int*)malloc(sizeof(int));
    if(!int_buf) return ERR;    // errno already set by the call
    char* pathname=NULL;     // will hold the pathname of the file to open
    int path_len;   // length of the pathname
    int flags;  // will hold the operation flags the client passed

    *int_buf=SUCCESS;   // the request has been accepted
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_open;

    // reading path string length
    if((readn(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_open;
    path_len=*int_buf;  // getting the pathname length
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_open;

    pathname=(char*)calloc(path_len, sizeof(char)); // allocating mem for the path string
    if(!pathname) goto cleanup_w_open_fatal;

    // reading the pathname
    if((readn(client_fd, (void*)pathname, sizeof(int)))==ERR) goto cleanup_w_open;
    if(!pathname) goto cleanup_w_open;
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_open;

    // reading operation flags
    if((readn(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_open;
    flags=*int_buf;
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_open;

    if(flags==0) {  // normal open
        if((temperr=sc_lookup(server_cache, pathname, OPEN_F, client_fd, NULL, NULL, 0))!=SUCCESS) {
            *int_buf=temperr;   // preparing the error message for the client
            writen(client_fd, (void*)int_buf, sizeof(int));
            goto cleanup_w_open;
        }
    }

    else if(flags==O_CREATE || flags==(O_CREATE|O_LOCK)) {  // open with create
        if((temperr=sc_lookup(server_cache, pathname, OPEN_C_F, client_fd, NULL, NULL, 0))!=SUCCESS) {
            *int_buf=temperr;   // preparing the error message for the client
            writen(client_fd, (void*)int_buf, sizeof(int));
            goto cleanup_w_open;
        }
    }

    if(flags==O_LOCK || flags==(O_CREATE|O_LOCK)) { // open with lock
        if((temperr=sc_lookup(server_cache, pathname, LOCK_F, client_fd, NULL, NULL, 0))!=SUCCESS) {
            *int_buf=temperr;   // preparing the error message for the client
            writen(client_fd, (void*)int_buf, sizeof(int));
            goto cleanup_w_open;
        }
    }

    *int_buf=SUCCESS;   // tell the client the operation succeeded
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_open;


    free(int_buf);
    free(pathname);
    return SUCCESS;

// ERROR CLEANUP
cleanup_w_open:
    if(int_buf) free(int_buf);
    if(pathname) free(pathname);
    return 0;   // client-side error, server can keep working
cleanup_w_open_fatal:
    if(int_buf) free(int_buf);
    if(pathname) free(pathname);
    return ERR;
}

