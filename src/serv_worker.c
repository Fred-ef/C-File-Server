#include "serv_worker.h"

// thread main
void* worker_func(void* arg) {


    pthread_exit((void*)SUCCESS);
}