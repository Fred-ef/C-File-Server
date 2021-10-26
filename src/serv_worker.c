#include "serv_worker.h"

// thread main
void* worker_func(void* arg) {

    int i;  // for loop index
    int temperr;    // used for error handling
    int* int_buf=NULL;   // serves as a buffer for int values
    int usr_fd;     // will hold the fd of the user that sent the request
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

        usr_fd=*int_buf;    // preparing to serve user's request
        free(int_buf);

        if((temperr=readn(usr_fd, int_buf, PIPE_MSG_LEN))==ERR)     // getting the request into int_buf
        {LOG_ERR(EPIPE, "worker: reading client operation"); exit(EXIT_FAILURE);}

        op=*int_buf;    // getting the operation code
        free(int_buf);

        if(op==EOF) {   // the client close the connection
            usr_fd*=-1; // tells the manager to close the connection with this client
            // TODO assiurarsi che funzioni
            if((temperr=writen(fd_pipe_write, (void*)(&usr_fd), PIPE_MSG_LEN))==ERR)
            {LOG_ERR(EPIPE, "worker: writing to the pipe");}
        }
        else if(op==OPEN_F) {

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

    pthread_exit((void*)SUCCESS);
}