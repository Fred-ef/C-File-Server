#include "serv_worker.h"

static int worker_file_open(int);
static int worker_file_read(int);
static int worker_file_readn(int);
static int worker_file_write(int);
static int worker_file_write_app(int);
static int worker_file_lock(int);
static int worker_file_unlock(int);
static int worker_file_remove(int);
static int worker_file_close(int);

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

        while(!(int_buf=(int*)conc_fifo_pop(requests_queue))) {  // checking for new requests
            if(errno) {LOG_ERR(errno, "worker: reading from req queue"); exit(EXIT_FAILURE);}

            if((temperr=pthread_cond_wait(&(requests_queue->queue_cv), &(requests_queue->queue_mtx)))==ERR)
            {LOG_ERR(temperr, "worker: waiting on req queue"); exit(EXIT_FAILURE);}
        }

        if((temperr=pthread_mutex_unlock(&(requests_queue->queue_mtx)))==ERR)
        {LOG_ERR(temperr, "worker: unlocking req queue"); exit(EXIT_FAILURE);}

        client_fd=*int_buf;    // preparing to serve the user's request
        free(int_buf);

        if((temperr=readn(client_fd, int_buf, PIPE_MSG_LEN))==ERR)     // getting the request into int_buf
        {LOG_ERR(EPIPE, "worker: reading client operation"); exit(EXIT_FAILURE);}

        op=*int_buf;    // reading the operation code
        free(int_buf);

        if(op==EOF) {   // the client closed the connection
            int_buf=&client_fd;
            client_fd*=-1; // tells the manager to close the connection with this client
            // TODO assiurarsi che funzioni
            if((temperr=writen(fd_pipe_write, (void*)int_buf, PIPE_MSG_LEN))==ERR)
            {LOG_ERR(EPIPE, "worker: writing to the pipe");}
        }
        else if(op==OPEN_F) {
            if((temperr=worker_file_open(client_fd))==ERR)
            {LOG_ERR(errno, "worker: open file error"); exit(EXIT_FAILURE);}

            *int_buf=client_fd;    // tells the manager the operation is complete
            if((temperr=writen(fd_pipe_write, (void*)int_buf, PIPE_MSG_LEN))==ERR)
            {LOG_ERR(EPIPE, "worker: writing to the pipe (open)");}
        }
        else if(op==READ_F) {
            if((temperr=worker_file_read(client_fd))==ERR)
            {LOG_ERR(errno, "worker: read file error"); exit(EXIT_FAILURE);}

            *int_buf=client_fd;    // tells the manager the operation is complete
            if((temperr=writen(fd_pipe_write, (void*)int_buf, PIPE_MSG_LEN))==ERR)
            {LOG_ERR(EPIPE, "worker: writing to the pipe (read)");}
        }
        else if(op==READ_N_F) {
            if((temperr=worker_file_readn(client_fd))==ERR)
            {LOG_ERR(errno, "worker: readn file error"); exit(EXIT_FAILURE);}

            *int_buf=client_fd;    // tells the manager the operation is complete
            if((temperr=writen(fd_pipe_write, (void*)int_buf, PIPE_MSG_LEN))==ERR)
            {LOG_ERR(EPIPE, "worker: writing to the pipe (readn)");}
        }
        else if(op==WRITE_F) {
            if((temperr=worker_file_write(client_fd))==ERR)
            {LOG_ERR(errno, "worker: write file error"); exit(EXIT_FAILURE);}

            *int_buf=client_fd;    // tells the manager the operation is complete
            if((temperr=writen(fd_pipe_write, (void*)int_buf, PIPE_MSG_LEN))==ERR)
            {LOG_ERR(EPIPE, "worker: writing to the pipe (write)");}
        }
        else if(op==WRITE_F_APP) {
            if((temperr=worker_file_write_app(client_fd))==ERR)
            {LOG_ERR(errno, "worker: write_app file error"); exit(EXIT_FAILURE);}

            *int_buf=client_fd;    // tells the manager the operation is complete
            if((temperr=writen(fd_pipe_write, (void*)int_buf, PIPE_MSG_LEN))==ERR)
            {LOG_ERR(EPIPE, "worker: writing to the pipe (write_app)");}
        }
        else if(op==LOCK_F) {
            if((temperr=worker_file_lock(client_fd))==ERR)
            {LOG_ERR(errno, "worker: lock file error"); exit(EXIT_FAILURE);}

            *int_buf=client_fd;    // tells the manager the operation is complete
            if((temperr=writen(fd_pipe_write, (void*)int_buf, PIPE_MSG_LEN))==ERR)
            {LOG_ERR(EPIPE, "worker: writing to the pipe (lock)");}
        }
        else if(op==UNLOCK_F) {
            if((temperr=worker_file_unlock(client_fd))==ERR)
            {LOG_ERR(errno, "worker: unlock file error"); exit(EXIT_FAILURE);}

            *int_buf=client_fd;    // tells the manager the operation is complete
            if((temperr=writen(fd_pipe_write, (void*)int_buf, PIPE_MSG_LEN))==ERR)
            {LOG_ERR(EPIPE, "worker: writing to the pipe (unlock)");}
        }
        else if(op==RM_F) {
            if((temperr=worker_file_remove(client_fd))==ERR)
            {LOG_ERR(errno, "worker: remove file error"); exit(EXIT_FAILURE);}

            *int_buf=client_fd;    // tells the manager the operation is complete
            if((temperr=writen(fd_pipe_write, (void*)int_buf, PIPE_MSG_LEN))==ERR)
            {LOG_ERR(EPIPE, "worker: writing to the pipe (remove)");}
        }
        else if(op==CLOSE_F) {
            if((temperr=worker_file_close(client_fd))==ERR)
            {LOG_ERR(errno, "worker: close file error"); exit(EXIT_FAILURE);}

            *int_buf=client_fd;    // tells the manager the operation is complete
            if((temperr=writen(fd_pipe_write, (void*)int_buf, PIPE_MSG_LEN))==ERR)
            {LOG_ERR(EPIPE, "worker: writing to the pipe (close)");}
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
    if(!pathname) return ERR;

    // reading the pathname
    if((readn(client_fd, (void*)pathname, path_len))==ERR) goto cleanup_w_open;
    if(!pathname) goto cleanup_w_open;
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_open;

    // reading operation flags
    if((readn(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_open;
    flags=*int_buf;
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_open;

    if(flags==0) {  // normal open
        if((temperr=sc_lookup(server_cache, pathname, OPEN_F, &client_fd, NULL, NULL, NULL))!=SUCCESS) {
            if(temperr==ERR) return ERR;
            *int_buf=temperr;   // preparing the error message for the client
            writen(client_fd, (void*)int_buf, sizeof(int));
            goto cleanup_w_open;
        }
    }

    else if(flags==O_CREATE || flags==(O_CREATE|O_LOCK)) {  // open with create
        if((temperr=sc_lookup(server_cache, pathname, OPEN_C_F, &client_fd, NULL, NULL, NULL))!=SUCCESS) {
            if(temperr==ERR) return ERR;
            *int_buf=temperr;   // preparing the error message for the client
            writen(client_fd, (void*)int_buf, sizeof(int));
            goto cleanup_w_open;
        }
    }

    if(flags==O_LOCK || flags==(O_CREATE|O_LOCK)) { // open with lock
        if((temperr=sc_lookup(server_cache, pathname, LOCK_F, &client_fd, NULL, NULL, NULL))!=SUCCESS) {
            if(temperr==ERR) return ERR;
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
    return -2;   // client-side error, server can keep working
}


static int worker_file_read(int client_fd) {
    int temperr;
    int* int_buf=(int*)malloc(sizeof(int));
    if(!int_buf) return ERR;    // errno already set by the call
    char* pathname=NULL;     // will hold the pathname of the file to read
    int path_len;   // length of the pathname

    byte* data_read=NULL;   // will contain the returned file
    int* bytes_read=NULL;  // will contain the size of the returned file

    *int_buf=SUCCESS;   // the request has been accepted
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_read;

    // reading path string length
    if((readn(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_read;
    path_len=*int_buf;  // getting the pathname length
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_read;

    pathname=(char*)calloc(path_len, sizeof(char)); // allocating mem for the path string
    if(!pathname) return ERR;

    // reading the pathname
    if((readn(client_fd, (void*)pathname, path_len))==ERR) goto cleanup_w_read;
    if(!pathname) goto cleanup_w_read;
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_read;

    // getting the file from the cache
    bytes_read=(int*)malloc(sizeof(int)); // allocating mem for the file size
    if(!bytes_read) return ERR;
    (*bytes_read)=0;    // setting a default value
    if((temperr=sc_lookup(server_cache, pathname, READ_F, &client_fd, &data_read, NULL, bytes_read))!=SUCCESS) {
        if(temperr==ERR) return ERR;
        *int_buf=temperr;   // preparing the error message for the client
        writen(client_fd, (void*)int_buf, sizeof(int));
        goto cleanup_w_read;
    }

    // communicating the size of the file to read
    if((writen(client_fd, (void*)bytes_read, sizeof(int)))==ERR) goto cleanup_w_read;

    // sending the file (ONLY IF it is not empty)
    if((*bytes_read))
        if((writen(client_fd, (void*)data_read, (*bytes_read)))==ERR) goto cleanup_w_read;
    

    if(int_buf) free(int_buf);
    if(pathname) free(pathname);
    if(data_read) free(data_read);
    if(bytes_read) free(bytes_read);
    return SUCCESS;

// ERROR CLEANUP
cleanup_w_read:
    if(int_buf) free(int_buf);
    if(pathname) free(pathname);
    if(data_read) free(data_read);
    if(bytes_read) free(bytes_read);
    return -2;   // client-side error, server can keep working
}


static int worker_file_readn(int client_fd) {
    int temperr;
    int* int_buf=(int*)malloc(sizeof(int));
    if(!int_buf) return ERR;    // errno already set by the call
    char* pathname=NULL;     // will hold the pathname of the file to read
    int path_len;   // length of the pathname

    byte* data_read=NULL;   // will contain the returned file
    int* bytes_read=NULL;  // will contain the size of the returned file

    *int_buf=SUCCESS;   // the request has been accepted
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_read;

    // reading path string length
    if((readn(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_read;
    path_len=*int_buf;  // getting the pathname length
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_read;

    pathname=(char*)calloc(path_len, sizeof(char)); // allocating mem for the path string
    if(!pathname) return ERR;

    // reading the pathname
    if((readn(client_fd, (void*)pathname, path_len))==ERR) goto cleanup_w_read;
    if(!pathname) goto cleanup_w_read;
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_read;

    // getting the file from the cache
    bytes_read=(int*)malloc(sizeof(int)); // allocating mem for the file size
    if(!bytes_read) return ERR;
    (*bytes_read)=0;    // setting a default value
    if((temperr=sc_lookup(server_cache, pathname, READ_F, &client_fd, &data_read, NULL, bytes_read))!=SUCCESS) {
        if(temperr==ERR) return ERR;
        *int_buf=temperr;   // preparing the error message for the client
        writen(client_fd, (void*)int_buf, sizeof(int));
        goto cleanup_w_read;
    }

    // communicating the size of the file to read
    if((writen(client_fd, (void*)bytes_read, sizeof(int)))==ERR) goto cleanup_w_read;

    // sending the file (ONLY IF it is not empty)
    if((*bytes_read))
        if((writen(client_fd, (void*)data_read, (*bytes_read)))==ERR) goto cleanup_w_read;
    

    if(int_buf) free(int_buf);
    if(pathname) free(pathname);
    if(data_read) free(data_read);
    if(bytes_read) free(bytes_read);
    return SUCCESS;

// ERROR CLEANUP
cleanup_w_read:
    if(int_buf) free(int_buf);
    if(pathname) free(pathname);
    if(data_read) free(data_read);
    if(bytes_read) free(bytes_read);
    return -2;   // client-side error, server can keep working
}

static int worker_file_write(int client_fd) {
    int temperr;
    int* int_buf=(int*)malloc(sizeof(int));
    if(!int_buf) return ERR;    // errno already set by the call
    char* pathname=NULL;     // will hold the pathname of the file to write
    int path_len;   // length of the pathname
    size_t bytes_written;    // dimension of the file
    byte* data_written=NULL;     // file data


    *int_buf=SUCCESS;   // the request has been accepted
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_write;

    // reading path string length
    if((readn(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_write;
    path_len=*int_buf;  // getting the pathname length
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_write;

    pathname=(char*)calloc(path_len, sizeof(char)); // allocating mem for the path string
    if(!pathname) goto cleanup_w_write;

    // reading the pathname
    if((readn(client_fd, (void*)pathname, path_len))==ERR) goto cleanup_w_write;
    if(!pathname) goto cleanup_w_write;
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_write;

    // reading the file size
    if((readn(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_write;
    bytes_written=*int_buf;
    *int_buf=SUCCESS;
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_write;

    // reading the file data
    data_written=(byte*)calloc(bytes_written, sizeof(byte));
    if(!data_written) return ERR;
    if((readn(client_fd, (void*)data_written, bytes_written))==ERR) goto cleanup_w_write;
    if(!data_written) goto cleanup_w_write;

    // writing the file into the cache
    if((temperr=sc_lookup(server_cache, pathname, WRITE_F, &client_fd, NULL, &data_written, &bytes_written))!=SUCCESS) {
        if(temperr==ERR) return ERR;
        *int_buf=temperr;   // preparing the error message for the client
        writen(client_fd, (void*)int_buf, sizeof(int));
        goto cleanup_w_write;
    }

    // returning a success message
    *int_buf=SUCCESS;
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_write;


    if(int_buf) free(int_buf);
    if(pathname) free(pathname);
    if(data_written) free(data_written);
    return SUCCESS;

// CLEANUP SECTION
cleanup_w_write:
    if(int_buf) free(int_buf);
    if(pathname) free(pathname);
    if(data_written) free(data_written);
    return -2;
}


static int worker_file_write_app(int client_fd) {
    int temperr;
    int* int_buf=(int*)malloc(sizeof(int));
    if(!int_buf) return ERR;    // errno already set by the call
    char* pathname=NULL;     // will hold the pathname of the file to write onto
    int path_len;   // length of the pathname
    size_t bytes_written;    // dimension of the write block
    byte* data_written=NULL;     // write block data


    *int_buf=SUCCESS;   // the request has been accepted
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_write_app;

    // reading path string length
    if((readn(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_write_app;
    path_len=*int_buf;  // getting the pathname length
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_write_app;

    pathname=(char*)calloc(path_len, sizeof(char)); // allocating mem for the path string
    if(!pathname) goto cleanup_w_write_app;

    // reading the pathname
    if((readn(client_fd, (void*)pathname, path_len))==ERR) goto cleanup_w_write_app;
    if(!pathname) goto cleanup_w_write_app;
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_write_app;

    // reading the write block size
    if((readn(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_write_app;
    bytes_written=*int_buf;
    *int_buf=SUCCESS;
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_write_app;

    // reading the write block data
    data_written=(byte*)calloc(bytes_written, sizeof(byte));
    if(!data_written) return ERR;
    if((readn(client_fd, (void*)data_written, bytes_written))==ERR) goto cleanup_w_write_app;
    if(!data_written) goto cleanup_w_write_app;

    // writing the file into the cache
    if((temperr=sc_lookup(server_cache, pathname, WRITE_F_APP, &client_fd, NULL, &data_written, &bytes_written))!=SUCCESS) {
        if(temperr==ERR) return ERR;
        *int_buf=temperr;   // preparing the error message for the client
        writen(client_fd, (void*)int_buf, sizeof(int));
        goto cleanup_w_write_app;
    }

    // returning a success message
    *int_buf=SUCCESS;
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_write_app;


    if(int_buf) free(int_buf);
    if(pathname) free(pathname);
    if(data_written) free(data_written);
    return SUCCESS;

// CLEANUP SECTION
cleanup_w_write_app:
    if(int_buf) free(int_buf);
    if(pathname) free(pathname);
    if(data_written) free(data_written);
    return -2;
}


static int worker_file_lock(int client_fd) {
    int temperr;
    int* int_buf=(int*)malloc(sizeof(int));
    if(!int_buf) return ERR;    // errno already set by the call
    char* pathname=NULL;     // will hold the pathname of the file to lock
    int path_len;   // length of the pathname


    *int_buf=SUCCESS;   // the request has been accepted
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_lock;

    // reading path string length
    if((readn(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_lock;
    path_len=*int_buf;  // getting the pathname length
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_lock;

    pathname=(char*)calloc(path_len, sizeof(char)); // allocating mem for the path string
    if(!pathname) goto cleanup_w_lock;

    // reading the pathname
    if((readn(client_fd, (void*)pathname, path_len))==ERR) goto cleanup_w_lock;
    if(!pathname) goto cleanup_w_lock;
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_lock;

    // performing the lock operation
    if((temperr=sc_lookup(server_cache, pathname, LOCK_F, &client_fd, NULL, NULL, NULL))!=SUCCESS) {
        if(temperr==ERR) return ERR;
        *int_buf=temperr;   // preparing the error message for the client
        writen(client_fd, (void*)int_buf, sizeof(int));
        goto cleanup_w_lock;
    }

    // returning a success message
    *int_buf=SUCCESS;
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_lock;


    if(int_buf) free(int_buf);
    if(pathname) free(pathname);
    return SUCCESS;

// CLEANUP SECTION
cleanup_w_lock:
    if(int_buf) free(int_buf);
    if(pathname) free(pathname);
    return -2;
}


static int worker_file_unlock(int client_fd) {
    int temperr;
    int* int_buf=(int*)malloc(sizeof(int));
    if(!int_buf) return ERR;    // errno already set by the call
    char* pathname=NULL;     // will hold the pathname of the file to unlock
    int path_len;   // length of the pathname


    *int_buf=SUCCESS;   // the request has been accepted
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_unlock;

    // reading path string length
    if((readn(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_unlock;
    path_len=*int_buf;  // getting the pathname length
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_unlock;

    pathname=(char*)calloc(path_len, sizeof(char)); // allocating mem for the path string
    if(!pathname) goto cleanup_w_unlock;

    // reading the pathname
    if((readn(client_fd, (void*)pathname, path_len))==ERR) goto cleanup_w_unlock;
    if(!pathname) goto cleanup_w_unlock;
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_unlock;

    // performing the unlock operation
    if((temperr=sc_lookup(server_cache, pathname, UNLOCK_F, &client_fd, NULL, NULL, NULL))!=SUCCESS) {
        if(temperr==ERR) return ERR;
        *int_buf=temperr;   // preparing the error message for the client
        writen(client_fd, (void*)int_buf, sizeof(int));
        goto cleanup_w_unlock;
    }

    // returning a success message
    *int_buf=SUCCESS;
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_unlock;


    if(int_buf) free(int_buf);
    if(pathname) free(pathname);
    return SUCCESS;

// CLEANUP SECTION
cleanup_w_unlock:
    if(int_buf) free(int_buf);
    if(pathname) free(pathname);
    return -2;
}


static int worker_file_remove(int client_fd) {
    int temperr;
    int* int_buf=(int*)malloc(sizeof(int));
    if(!int_buf) return ERR;    // errno already set by the call
    char* pathname=NULL;     // will hold the pathname of the file to remove
    int path_len;   // length of the pathname


    *int_buf=SUCCESS;   // the request has been accepted
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_rm;

    // reading path string length
    if((readn(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_rm;
    path_len=*int_buf;  // getting the pathname length
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_rm;

    pathname=(char*)calloc(path_len, sizeof(char)); // allocating mem for the path string
    if(!pathname) goto cleanup_w_rm;

    // reading the pathname
    if((readn(client_fd, (void*)pathname, path_len))==ERR) goto cleanup_w_rm;
    if(!pathname) goto cleanup_w_rm;
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_rm;

    // performing the remove operation
    if((temperr=sc_lookup(server_cache, pathname, RM_F, &client_fd, NULL, NULL, NULL))!=SUCCESS) {
        if(temperr==ERR) return ERR;
        *int_buf=temperr;   // preparing the error message for the client
        writen(client_fd, (void*)int_buf, sizeof(int));
        goto cleanup_w_rm;
    }

    // returning a success message
    *int_buf=SUCCESS;
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_rm;


    if(int_buf) free(int_buf);
    if(pathname) free(pathname);
    return SUCCESS;

// CLEANUP SECTION
cleanup_w_rm:
    if(int_buf) free(int_buf);
    if(pathname) free(pathname);
    return -2;
}


static int worker_file_close(int client_fd) {
    int temperr;
    int* int_buf=(int*)malloc(sizeof(int));
    if(!int_buf) return ERR;    // errno already set by the call
    char* pathname=NULL;     // will hold the pathname of the file to close
    int path_len;   // length of the pathname


    *int_buf=SUCCESS;   // the request has been accepted
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_close;

    // reading path string length
    if((readn(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_close;
    path_len=*int_buf;  // getting the pathname length
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_close;

    pathname=(char*)calloc(path_len, sizeof(char)); // allocating mem for the path string
    if(!pathname) goto cleanup_w_close;

    // reading the pathname
    if((readn(client_fd, (void*)pathname, path_len))==ERR) goto cleanup_w_close;
    if(!pathname) goto cleanup_w_close;
    *int_buf=SUCCESS;   // step OK
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_close;

    // performing the close operation
    if((temperr=sc_lookup(server_cache, pathname, CLOSE_F, &client_fd, NULL, NULL, NULL))!=SUCCESS) {
        if(temperr==ERR) return ERR;
        *int_buf=temperr;   // preparing the error message for the client
        writen(client_fd, (void*)int_buf, sizeof(int));
        goto cleanup_w_close;
    }

    // returning a success message
    *int_buf=SUCCESS;
    if((writen(client_fd, (void*)int_buf, sizeof(int)))==ERR) goto cleanup_w_close;


    if(int_buf) free(int_buf);
    if(pathname) free(pathname);
    return SUCCESS;

// CLEANUP SECTION
cleanup_w_close:
    if(int_buf) free(int_buf);
    if(pathname) free(pathname);
    return -2;
}