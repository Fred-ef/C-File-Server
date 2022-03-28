#include "serv_logger.h"

// thread main
void* logger_func(void* arg) {
    int temperr;
    int logfile_fd;
    char* buffer=NULL;

    while(!hard_close) {

        if((temperr=pthread_mutex_lock(&(log_queue->queue_mtx)))==ERR)
        {LOG_ERR(temperr, "logger: locking log queue"); exit(EXIT_FAILURE);}

        while(!(buffer=(char*)conc_fifo_pop(log_queue)) && !(hard_close)) {  // checking for new logs
            if(errno) {LOG_ERR(errno, "logger: reading from log queue"); exit(EXIT_FAILURE);}

            if((temperr=pthread_cond_wait(&(log_queue->queue_cv), &(log_queue->queue_mtx)))==ERR)
            {LOG_ERR(temperr, "logger: waiting on log queue"); exit(EXIT_FAILURE);}
        }

        if((temperr=pthread_mutex_unlock(&(log_queue->queue_mtx)))==ERR)
        {LOG_ERR(temperr, "logger: unlocking log queue"); exit(EXIT_FAILURE);}

        if(hard_close) break;   // server must terminate immediately

        if((logfile_fd=open(log_file_path, O_WRONLY | O_APPEND | O_CREAT, 0777))==ERR) {
            LOG_ERR(errno, "logger: opening log file");
            exit(EXIT_FAILURE);
        }

        if((write(logfile_fd, (void*)buffer, strlen(buffer)))==ERR) {
            LOG_ERR(errno, "logger: writing on log file");
            exit(EXIT_FAILURE);
        }

        if((close(logfile_fd))==ERR) {
            LOG_ERR(errno, "logger: closing log file");
            exit(EXIT_FAILURE);
        }


        if(buffer) {free(buffer); buffer=NULL;}
    }


    if(buffer) free(buffer);
    return NULL;
}