/* AGGIUNGERE DESCRIZIONE SERVER MANAGER THREAD */

#ifndef serv_logger_h
#define serv_logger_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "serv_manager.h"



// define for logging operations
#define LOG_OPERATION(fmt, ...) \
    { \
        char* buffer=(char*)calloc(1024, sizeof(char)); \
        if(!buffer) {LOG_ERR(temperr, "logging: allocating buffer"); return ERR;} \
        memset(buffer, '\0', 1024); \
        sprintf(buffer, fmt, ##__VA_ARGS__); \
        if((temperr=pthread_mutex_lock(&(log_queue->queue_mtx)))==ERR) { \
            LOG_ERR(temperr, "logging: locking log queue"); return ERR; \
        } \
        if((temperr=conc_fifo_push(log_queue, (void*)buffer))==ERR) { \
            LOG_ERR(errno, "logging: pushing log into queue"); return ERR; \
        } \
        if((temperr=pthread_cond_signal(&(log_queue->queue_cv)))==ERR) { \
            LOG_ERR(temperr, "logging: signaling log to logger"); return ERR; \
        } \
        if((temperr=pthread_mutex_unlock(&(log_queue->queue_mtx)))==ERR) { \
            LOG_ERR(temperr, "logging: unlocking log queue"); return ERR; \
        } \
    }



void* logger_func(void* arg);   // main thread function

#endif // serv_logger_h