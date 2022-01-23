#include "client_API.h"             

short fd_sock=-1;      // initializing the socket fd to -1 (will return error if used)
char* serv_sk=NULL;      // initializing the server-address note to NULL (will return error if used)
bool is_connected=0;     // set to 1 when the connection with the server is established, 0 otherwise

// Opens the connection to the server
int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    if(!sockname) {errno=EINVAL; return OP_ERR;}

    short errtemp;      // used for error-testing in syscalls
    short timeout;      // used for timeout expiration checking

    stimespec* timer=NULL;       // implements the 200ms try-again timer
    stimespec* rem_time=NULL;        // auxiliary structure, in case of sleep-interruption

    timer=(stimespec*)malloc(sizeof(stimespec));
    if(!timer) {errno=ENOMEM; return ERR;}

    rem_time=(stimespec*)malloc(sizeof(stimespec));
    if(!rem_time) {errno=ENOMEM; return ERR;}

    timer->tv_sec=0;        // no need for seconds!
    timer->tv_nsec=(MS_TO_NS(RECONNECT_TIMER));     // setting the timer with the proper value

    struct sockaddr_un sa;      // server socket address

    serv_sk=calloc(strlen(sockname), sizeof(char));       // memorizes the server's address in order to modify the connection later TODO memerr
    if(!serv_sk) {errno=ENOMEM; return ERR;}

    strcpy(serv_sk, sockname);        // writes the sock address in a global variable to store it
    strcpy(sa.sun_path, sockname);       // writes the sock address in the socket adress structure
    sa.sun_family = AF_UNIX;        // specifies the socket-family used (IPC communication)

    if((fd_sock=socket(AF_UNIX, SOCK_STREAM, 0))==ERR) goto cleanup_connect;

    while((errtemp=connect(fd_sock, (struct sockaddr*)&sa, sizeof(sa)))==ERR && (timeout=compare_current_time(&abstime)) != -1) {     // connecting to the server socket
        if(errno==ENOENT) {
            if((errtemp=sleep_ms(RECONNECT_TIMER))==ERR) return ERR;
        }     // wait, then try again
        else goto cleanup_connect;
    }

    if(timeout == -1) {errno=ETIME; goto cleanup_connect;}      // could not connect before timer expiration


    is_connected=1;
    cleanup_pointers((void*)timer, (void*)rem_time, (void*)serv_sk, NULL);        // cleaning all the allocated memory
    return SUCCESS;     // the client has successfully connected to the file server

cleanup_connect:        // executes in case something went wrong and the operation cannot be successfully completed
    cleanup_pointers((void*)timer, (void*)rem_time, (void*)serv_sk, NULL);        // cleaning all the allocated memory
    return OP_ERR;     // informs the caller that the operation failed
}


int closeConnection(const char* sockname) {
    if(!sockname) {errno=EINVAL; goto cleanup_close;}

    short errtemp;

    if(strcmp(sockname, serv_sk) != 0) {      // if the connection to close is different from the connection that is open, fail
        errno=EINVAL;       // setting the error number to "invalid argument"
        goto cleanup_close;
    }

    if((errtemp=close(fd_sock))==ERR) return ERR;       // couldn't close socket


    if(serv_sk) free(serv_sk);  // freeing the sock address
    return SUCCESS;     // the client has successfully closed the connection to the file server

// CLEANUP SECTION
cleanup_close:
    if(serv_sk) free(serv_sk);
    return OP_ERR;
}


int openFile(const char* pathname, int flags) {
    if(!pathname) {errno=EINVAL; goto cleanup_open;}       // pathname cannot be NULL
    if(is_connected==0) {errno=ECONNREFUSED; goto cleanup_open;}      // the client must be connected to send requests

    op_code op=OPEN_F;        // sets the operation code to openFile
    int* int_buf=(int*)malloc(sizeof(int));        // will hold certain server responses for error detection
    if(!int_buf) return ERR;

    // communicates operation
    *int_buf=op;
    writen(fd_sock, (void*)int_buf, sizeof(int));     // tells the server what operation to execute
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_open;}       // if the operation prep failed for some reason, abort the operation

    // communicates pathname lenght
    *int_buf=strlen(pathname);
    writen(fd_sock, (void*)int_buf, sizeof(int));      // tells the server what read size to expect next
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_open;}       // if the server couldn't process the size, abort the operation

    // communicates pathname
    writen(fd_sock, (void*)pathname, sizeof(pathname));        // tells the server what file to open
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_open;}       // if the server could not get the path, abort the operation

    // communicates flags
    *int_buf=flags;
    writen(fd_sock, (void*)int_buf, sizeof(int));     // tells the server what flags to use in the open
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_open;}       // if the flags are invalid, abort the operation

    readn(fd_sock, (void*)int_buf, sizeof(int));    // reads the operation result value
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_open;}       // if the operation failed, return error


    if(int_buf) free(int_buf);
    return SUCCESS;       // the file has been opened on the server

// CLEANUP SECTION
cleanup_open:
    if(int_buf) free(int_buf);
    return OP_ERR;
}


int readFile(const char* pathname, void** buf, size_t* size) {
    if(!pathname || !size || !buf) {errno=EINVAL; goto cleanup_read;}       // args cannot be NULL
    if(is_connected==0) {errno=ECONNREFUSED; goto cleanup_read;}      // the client must be connected to send requests

    op_code op=READ_F;        // sets the operation code to readFile
    int* int_buf=(int*)malloc(sizeof(int));        // will hold certain server responses for error detection
    if(!int_buf) return ERR;

    // communicates operation
    *int_buf=op;
    writen(fd_sock, (void*)int_buf, sizeof(int));     // tells the server what operation to execute
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_read;}       // if the operation prep failed for some reason, abort the operation

    // communicates pathname lenght
    *int_buf=strlen(pathname);
    writen(fd_sock, (void*)int_buf, sizeof(int));      // tells the server what read size to expect next
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_read;}       // if the server couldn't process the size, abort the operation

    // communicates pathname
    writen(fd_sock, (void*)pathname, sizeof(pathname));        // tells the server what file to open
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_read;}       // if the server could not get the path, abort the operation

    // receives the size of the file to read
    size=(size_t*)malloc(sizeof(size_t));
    if(!size) return ERR;
    readn(fd_sock, (void*)size, sizeof(size_t));     // gets the size of the file to read
    if(*size < 0) {errno=EIO; goto cleanup_read;}

    if(size==0) (*buf)=NULL;    // empty file, return null as its content
    else {  // receives the file requested
        (*buf)=(void*)malloc((*size)*sizeof(byte));
        if(!buf) return ERR;
        readn(fd_sock, (*buf), (*size));    // gets the actual file content
        if(!(*buf)) {errno=EIO; goto cleanup_read;}
    }

    if(int_buf) free(int_buf);
    return SUCCESS;       // the file has been opened on the server

// CLEANUP SECTION
cleanup_read:
    if(int_buf) free(int_buf);
    if(size) free(size);
    if(*buf) free(*buf);
    return OP_ERR;
}

int readNFiles(int N, const char* dirname) {
    if(!dirname) {errno=EINVAL; goto cleanup_readn;}

    op_code op = READ_N_F;     // sets the operation code to readNFiles
    int* int_buf=(int*)malloc(sizeof(int));        // will hold certain server responses for error detection
    if(!int_buf) return ERR;
    size_t* size=NULL;      // will contain the size of the current file
    byte* buf=NULL;     // will contain the actual current file
    char* file_name=NULL;       // will contain the name of the current file
    short fd;   // will contain the fd of the current file
    char* pathname=NULL;     // will contain the final path of the file

    // communicates operation
    *int_buf=op;
    writen(fd_sock, (void*)int_buf, sizeof(int));     // tells the server what operation to execute
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_readn;} 

    // receives the number of files to read
    readn(fd_sock, (void*)int_buf, sizeof(int));     // gets the size of the file to read
    if(*int_buf < 0) {errno=*int_buf; goto cleanup_readn;}

    // updating the number of files to read
    N = *int_buf;

    if(N>0) {     // if there is at least one file to read
        while(N>0) {    // repeat until there are no more files to read

            // receives the size of the name of the file
            size=(size_t*)malloc(sizeof(size_t));
            if(!size) return ERR;
            readn(fd_sock, (void*)size, sizeof(size_t));     // gets the size of the file to read
            if(*size < 0) {errno=EIO; break;}

            // receives the name of the file
            file_name=(char*)malloc((*size)*sizeof(char));
            if(!file_name) return ERR;
            readn(fd_sock, (void*)file_name, size);     // gets the size of the file to read
            if(*size < 0) {errno=EIO; break;}
            free(size);

            // receives the size of the file to read
            size=(size_t*)malloc(sizeof(size_t));
            if(!size) return ERR;
            readn(fd_sock, (void*)size, sizeof(size_t));     // gets the size of the file to read
            if(*size < 0) {errno=EIO; break;}

            // reading the file
            if(size==0) (*buf)=NULL;    // empty file, return null as its content
            else {  // receives the file requested
                (*buf)=(void*)malloc((*size)*sizeof(byte));
                if(!buf) return ERR;
                readn(fd_sock, (*buf), (*size));    // gets the actual file content
                if(!(*buf)) {errno=EIO; break;}
            }

            // creating the file in the specified dir
            pathname = build_path_name(dirname, file_name);
            if((fd=open(pathname, O_WRONLY, O_CREAT))==ERR) break;
            if((write(fd, buf, size))==ERR) break;
            if((close(fd))==ERR) return ERR;


            free(size);
            free(file_name);
            free(buf);  // freeing buf to read next file
            free(pathname);
            N++;
        }
    }

    if(int_buf) free(int_buf);
    if(size) free(size);
    if(file_name) free(file_name);
    if(buf) free(buf);
    if(pathname) free(pathname);
    return N;

// CLEANUP SECTION
cleanup_readn:
    if(int_buf) free(int_buf);
    if(size) free(size);
    if(file_name) free(file_name);
    if(buf) free(buf);
    if(pathname) free(pathname);
    return OP_ERR;
}


int lockFile(const char* pathname) {
    if(pathname==NULL) {errno=EINVAL; goto cleanup_lock;}       // pathname cannot be NULL
    if(is_connected==0) {errno=ECONNREFUSED; goto cleanup_lock;}      // the client must be connected to send requests

    op_code op=LOCK_F;        // sets the operation code to lockFile
    int* int_buf=(int*)malloc(sizeof(int));        // will hold certain server responses for error detection
    if(!int_buf) return ERR;
    int temperr;

    do {  // if the lock isn't immediately available, sleep and try again later
        // communicates operation
        *int_buf=op;
        writen(fd_sock, (void*)int_buf, sizeof(int));     // tells the server what operation to execute
        readn(fd_sock, (void*)int_buf, sizeof(int));
        if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_lock;}       // if the operation prep failed for some reason, abort the operation

        // communicates pathname lenght
        *int_buf=strlen(pathname);
        writen(fd_sock, (void*)int_buf, sizeof(int));      // tells the server what read size to expect next
        readn(fd_sock, (void*)int_buf, sizeof(int));
        if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_lock;}       // if the server couldn't process the size, abort the operation

        // communicates pathname
        writen(fd_sock, (void*)pathname, sizeof(pathname));        // tells the server what file to lock
        readn(fd_sock, (void*)int_buf, sizeof(int));
        if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_lock;}       // if the server could not get the path, abort the operation

        readn(fd_sock, (void*)int_buf, sizeof(int));    // reads the operation result value
        if((*int_buf)!=SUCCESS && (*int_buf)!=EBUSY) {errno=*int_buf; goto cleanup_lock;}       // if the operation failed, return error

        if(*int_buf!=SUCCESS) {if((temperr=sleep_ms(LOCK_TIMER))==ERR) return ERR;} // sleeps before asking again for the lock
    } while((*int_buf)==EBUSY);

    if(int_buf) free(int_buf);
    return SUCCESS;

// CLEANUP SECTION
cleanup_lock:
    if(int_buf) free(int_buf);
    return OP_ERR;
}


int unlockFile(const char* pathname) {
    if(pathname==NULL) {errno=EINVAL; goto cleanup_unlock;}       // pathname cannot be NULL
    if(is_connected==0) {errno=ECONNREFUSED; goto cleanup_unlock;}      // the client must be connected to send requests

    op_code op=UNLOCK_F;        // sets the operation code to unlockFile
    int* int_buf=(int*)malloc(sizeof(int));        // will hold certain server responses for error detection
    if(!int_buf) return ERR;

    // communicates operation
    *int_buf=op;
    writen(fd_sock, (void*)int_buf, sizeof(int));     // tells the server what operation to execute
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_unlock;}       // if the operation prep failed for some reason, abort the operation

    // communicates pathname lenght
    *int_buf=strlen(pathname);
    writen(fd_sock, (void*)int_buf, sizeof(int));      // tells the server what read size to expect next
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_unlock;}       // if the server couldn't process the size, abort the operation

    // communicates pathname
    writen(fd_sock, (void*)pathname, sizeof(pathname));        // tells the server what file to unlock
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_unlock;}       // if the server could not get the path, abort the operation

    readn(fd_sock, (void*)int_buf, sizeof(int));    // reads the operation result value
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_unlock;}       // if the operation failed, return error


    if(int_buf) free(int_buf);
    return SUCCESS;

// CLEANUP SECTION
cleanup_unlock:
    if(int_buf) free(int_buf);
    return OP_ERR;
}


int closeFile(const char* pathname) {
    if(pathname==NULL) {errno=EINVAL; goto cleanup_close;}       // pathname cannot be NULL
    if(is_connected==0) {errno=ECONNREFUSED; goto cleanup_close;}      // the client must be connected to send requests

    op_code op=CLOSE_F;        // sets the operation code to closeFile
    int* int_buf=(int*)malloc(sizeof(int));        // will hold certain server responses for error detection
    if(!int_buf) return ERR;

    // communicates operation
    *int_buf=op;
    writen(fd_sock, (void*)int_buf, sizeof(int));     // tells the server what operation to execute
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_close;}       // if the operation prep failed for some reason, abort the operation

    // communicates pathname lenght
    *int_buf=strlen(pathname);
    writen(fd_sock, (void*)int_buf, sizeof(int));      // tells the server what read size to expect next
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_close;}       // if the server couldn't process the size, abort the operation

    // communicates pathname
    writen(fd_sock, (void*)pathname, sizeof(pathname));        // tells the server what file to close
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_close;}       // if the server could not get the path, abort the operation

    readn(fd_sock, (void*)int_buf, sizeof(int));    // reads the operation result value
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_close;}       // if the operation failed, return error


    if(int_buf) free(int_buf);
    return SUCCESS;

// CLEANUP SECTION
cleanup_close:
    if(int_buf) free(int_buf);
    return OP_ERR;
}


int removeFile(const char* pathname) {
    if(pathname==NULL) {errno=EINVAL; goto cleanup_close;}       // pathname cannot be NULL
    if(is_connected==0) {errno=ECONNREFUSED; goto cleanup_close;}      // the client must be connected to send requests

    op_code op=RM_F;        // sets the operation code to closeFile
    int* int_buf=(int*)malloc(sizeof(int));        // will hold certain server responses for error detection
    if(!int_buf) return ERR;

    // communicates operation
    *int_buf=op;
    writen(fd_sock, (void*)int_buf, sizeof(int));     // tells the server what operation to execute
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_close;}       // if the operation prep failed for some reason, abort the operation

    // communicates pathname lenght
    *int_buf=strlen(pathname);
    writen(fd_sock, (void*)int_buf, sizeof(int));      // tells the server what read size to expect next
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_close;}       // if the server couldn't process the size, abort the operation

    // communicates pathname
    writen(fd_sock, (void*)pathname, sizeof(pathname));        // tells the server what file to remove
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_close;}       // if the server could not get the path, abort the operation

    readn(fd_sock, (void*)int_buf, sizeof(int));    // reads the operation result value
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_close;}       // if the operation failed, return error


    if(int_buf) free(int_buf);
    return SUCCESS;

// CLEANUP SECTION
cleanup_close:
    if(int_buf) free(int_buf);
    return OP_ERR;
}


/* #################################################################################################### */
/* ######################################### HELPER FUNCTIONS ######################################### */
/* #################################################################################################### */


static short sleep_ms(int duration) {
    short errtemp;
    stimespec* timer=(stimespec*)malloc(sizeof(stimespec));
    if(!timer) {errno=ENOMEM; return ERR;}
    timer->tv_sec=0;
    timer->tv_nsec=(MS_TO_NS(duration));

    
    if((errtemp=nanosleep_w(timer))==ERR) {
        return ERR;
    }

    return SUCCESS;
}

char* build_path_name(char* dir, char* name) {
    if(!dir || !name) return NULL;
    int dirlen = strlen(dir);
    int namelen = strlen(name);
    if(dirlen+namelen >= 1024-1) return NULL;

    char* path_name = (char*)malloc(1024*sizeof(char));
    if(!path_name) return NULL;
    
    strncpy(path_name, dir, dirlen);

    if(path_name[dirlen-1] != '/') {
        path_name[dirlen] = '/';
        path_name[dirlen+1] = '\0';
    }
    
    strncpy(path_name, name, namelen);
    // TODO remove excess '/' from the name

    return path_name;
}