#include "client_API.h"             

short fd_sock=-1;      // initializing the socket fd to -1 (will return error if used)
char* serv_sk=NULL;      // initializing the server-address note to NULL (will return error if used)
bool is_connected=0;     // set to 1 when the connection with the server is established, 0 otherwise

char* save_dir=NULL;    // used to specify the folder in which to save files retrieved from the file-server
char* miss_dir=NULL;    // used to specify the folder in which to save files discarded by the file-server

// Opens the connection to the server
int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    if(!sockname) {errno=EINVAL; return ERR;}
    errno=0;

    short errtemp;      // used for error-testing in syscalls
    short timeout=0;      // used for timeout expiration checking

    stimespec* timer=NULL;       // implements the 200ms try-again timer
    stimespec* rem_time=NULL;        // auxiliary structure, in case of sleep-interruption

    timer=(stimespec*)malloc(sizeof(stimespec));
    if(!timer) return ERR;

    rem_time=(stimespec*)malloc(sizeof(stimespec));
    if(!rem_time) return ERR;

    timer->tv_sec=0;        // no need for seconds!
    timer->tv_nsec=(MS_TO_NS(RECONNECT_TIMER));     // setting the timer with the proper value

    struct sockaddr_un sa;      // server socket address

    serv_sk=calloc(strlen(sockname)+1, sizeof(char));       // memorizes the server's address in order to modify the connection later
    if(!serv_sk) return ERR;

    strcpy(serv_sk, sockname);        // writes the sock address in a global variable to store it
    strcpy(sa.sun_path, sockname);       // writes the sock address in the socket adress structure
    sa.sun_family = AF_UNIX;        // specifies the socket-family used (IPC communication)

    if((fd_sock=socket(AF_UNIX, SOCK_STREAM, 0))==ERR) goto cleanup_connect;

    while((errtemp=connect(fd_sock, (struct sockaddr*)&sa, sizeof(sa)))==ERR && (timeout=compare_current_time(&abstime)) != -1) {     // connecting to the server socket
        if(errno==ENOENT) {
            if((errtemp=usleep(US_TO_MS(RECONNECT_TIMER)))==ERR)
            {LOG_ERR(errno, "sleeping in reconnect interval"); return ERR;}
        }     // wait, then try again
        else goto cleanup_connect;
    }

    if(timeout == -1) {errno=ETIME; goto cleanup_connect;}      // could not connect before timer expiration


    is_connected=1;
    if(timer) free(timer);
    if(rem_time) free(rem_time);
    return SUCCESS;     // the client has successfully connected to the file server

cleanup_connect:        // executes in case something went wrong and the operation cannot be successfully completed
    if(timer) free(timer);
    if(rem_time) free(rem_time);
    return ERR;     // informs the caller that the operation failed
}


int closeConnection(const char* sockname) {
    if(!sockname) {errno=EINVAL; goto cleanup_close;}
    errno=0;

    short errtemp;

    if(strcmp(sockname, serv_sk) != 0) {      // if the connection to close is different from the connection that is open, fail
        errno=EINVAL;       // setting the error number to "invalid argument"
        goto cleanup_close;
    }

    if((errtemp=close(fd_sock))==ERR) return ERR;       // couldn't close socket


    is_connected=0;     // setting the connection flag to false
    if(serv_sk) {free(serv_sk); serv_sk=NULL;}  // freeing the sock address
    return SUCCESS;     // the client has successfully closed the connection to the file server

// CLEANUP SECTION
cleanup_close:
    if(serv_sk) {free(serv_sk); serv_sk=NULL;}
    return ERR;
}


int openFile(const char* pathname, int flags) {
    if(!pathname) {errno=EINVAL; return ERR;}       // pathname cannot be NULL
    if(is_connected==0) {errno=ECONNREFUSED; return ERR;}      // the client must be connected to send requests
    errno=0;

    op_code op=OPEN_F;        // sets the operation code to openFile
    int* int_buf=(int*)malloc(sizeof(int));        // will hold certain server responses for error detection
    if(!int_buf) return ERR;

    int i;
    int res=SUCCESS;    // will hold the result of the operation
    unsigned subst_files_num=0;     // will hold the number of files expelled from the cache
    size_t subst_files_name_len=0;    // will hold the length of the name of each of the files expelled (one at a time)
    char* subst_files_name=NULL;    // will hold the name of each of the files expelled (one at a time)
    size_t subst_files_size=0;    // will hold the size of each of the files expelled (one at a time)
    byte* subst_files_data=NULL;    // will hold the data of each of the files expelled (one at a time)
    char* final_path=NULL;  // will hold the path to save each of the the expelled file (one at a time)

    // communicates operation
    *int_buf=op;
    writen(fd_sock, (void*)int_buf, sizeof(int));     // tells the server what operation to execute
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_open;}       // if the operation prep failed for some reason, abort the operation

    // communicates pathname lenght
    *int_buf=strlen(pathname);
    writen(fd_sock, (void*)int_buf, sizeof(int));      // tells the server what read size to expect next
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_open;}

    // communicates pathname
    writen(fd_sock, (void*)pathname, strlen(pathname));        // tells the server what file to open
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_open;}       // if the server could not get the path, abort the operation

    // communicates flags
    *int_buf=flags;
    writen(fd_sock, (void*)int_buf, sizeof(int));     // tells the server what flags to use in the open
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_open;}       // if the flags are invalid, abort the operation

    readn(fd_sock, (void*)int_buf, sizeof(int));    // reads the operation result value
    if((*int_buf)!=SUCCESS) res=*int_buf;   // if the operation failed, store the err


    // ##### RETRIEVING SUBSTITUTED FILES #####

    // receives the number of expelled files
    readn(fd_sock, (void*)&subst_files_num, sizeof(unsigned));
    if(subst_files_num<0) {errno=res; goto cleanup_open;}

    // receiving the expelled files
    for(i=0; i<subst_files_num; i++) {
        subst_files_name_len=0;
        readn(fd_sock, (void*)&subst_files_name_len, sizeof(size_t));    // reading pathname length
        if(subst_files_name_len<0) {errno=EILSEQ; goto cleanup_open;}
        subst_files_name=(char*)calloc(subst_files_name_len+1, sizeof(char));
        if(!subst_files_name) return ERR;   // fatal error
        memset(subst_files_name, '\0', subst_files_name_len+1);
        readn(fd_sock, (void*)subst_files_name, subst_files_name_len);     // reading pathname
        readn(fd_sock, (void*)&subst_files_size, sizeof(size_t));     // reading file size
        if(subst_files_size<0) {errno=EILSEQ; goto cleanup_open;}
        if(subst_files_size) {
            subst_files_data=(byte*)calloc(subst_files_size, sizeof(byte));
            if(!subst_files_data) return ERR;   // fatal error
            readn(fd_sock, (void*)subst_files_data, subst_files_size);      // reading file
        }

        // if the miss dir is set, save the file in it
        if(miss_dir) {
            int temp_fd;
            final_path=(char*)calloc(UNIX_PATH_MAX, sizeof(char));
            if(!final_path) return ERR;  // fatal error
            memset(final_path, '\0', UNIX_PATH_MAX);
            strncat(final_path, miss_dir, UNIX_PATH_MAX-strlen(final_path));
            strncat(final_path, basename(subst_files_name), UNIX_PATH_MAX-strlen(final_path));
            if((temp_fd=open(final_path, O_WRONLY | O_APPEND | O_CREAT, 0777))==ERR) {
                goto cleanup_open;
            }
            if(subst_files_size) {
                if((write(temp_fd, (void*)subst_files_data, subst_files_size))==ERR) {
                    if((close(temp_fd))==ERR) return ERR;
                    goto cleanup_open;
                }
            }
            if((close(temp_fd))==ERR) return ERR;
            if(final_path) {free(final_path); final_path=NULL;}
        }
        // cleaning pointers
        if(subst_files_name) {free(subst_files_name); subst_files_name=NULL;}
        if(subst_files_data) {free(subst_files_data); subst_files_data=NULL;}
    }


    // the open has failed, but the expelled files have been retrieved correctly
    if(res!=SUCCESS) goto cleanup_open;

    if(int_buf) free(int_buf);
    return SUCCESS;       // the file has been opened on the server

// CLEANUP SECTION
cleanup_open:
    if(int_buf) free(int_buf);
    if(subst_files_name) free(subst_files_name);
    if(subst_files_data) free(subst_files_data);
    if(final_path) free(final_path);
    if(res) errno=res;
    return ERR;
}


int readFile(const char* pathname, void** buf, size_t* size) {
    if(!pathname || !buf) {errno=EINVAL; return ERR;}       // args cannot be NULL
    if(is_connected==0) {errno=ECONNREFUSED; return ERR;}      // the client must be connected to send requests
    errno=0;

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
    writen(fd_sock, (void*)pathname, strlen(pathname));        // tells the server what file to open
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_read;}       // if the server could not get the path, abort the operation

    readn(fd_sock, (void*)int_buf, sizeof(int));    // reads the operation result value
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_read;}   // if the operation failed, return error

    // receives the size of the file to read
    readn(fd_sock, (void*)size, sizeof(size_t));     // gets the size of the file to read
    if(*size < 0) {errno=EIO; goto cleanup_read;}

    if(*size==0) (*buf)=NULL;    // empty file, return null as its content
    else {  // receives the file requested
        (*buf)=(void*)calloc((*size), sizeof(byte));
        if(!buf) return ERR;
        readn(fd_sock, (void*)(*buf), (*size));    // gets the actual file content
        if(!(*buf)) {errno=EIO; goto cleanup_read;}
    }

    if(int_buf) free(int_buf);
    return SUCCESS;       // the file has been opened on the server

// CLEANUP SECTION
cleanup_read:
    if(int_buf) free(int_buf);
    if(*buf) free(*buf);
    return ERR;
}

int readNFiles(int N, const char* dirname) {
    if(is_connected==0) {errno=ECONNREFUSED; return ERR;}      // the client must be connected to send requests
    errno=0;

    op_code op = READ_N_F;     // sets the operation code to readNFiles
    int* int_buf=(int*)malloc(sizeof(int));        // will hold certain server responses for error detection
    if(!int_buf) return ERR;

    int i;     // for loop index
    int res=SUCCESS;    // temp result
    unsigned returned_files_num=0;  // will hold the number of returned files
    size_t returned_files_name_len=0;    // will hold the length of the name of each of the files returned (one at a time)
    char* returned_files_name=NULL;    // will hold the name of each of the files returned (one at a time)
    size_t returned_files_size=0;    // will hold the size of each of the files returned (one at a time)
    byte* returned_files_data=NULL;    // will hold the data of each of the files returned (one at a time)
    char* final_path=NULL;  // will hold the path to save each of the the returned file (one at a time)


    // communicates operation
    *int_buf=op;
    writen(fd_sock, (void*)int_buf, sizeof(int));     // tells the server what operation to execute
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_readn;}

    // communicates the maximum number of files to read
    *int_buf=N;
    writen(fd_sock, (void*)int_buf, sizeof(int));   // tells the server how many files to read at max
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_readn;}

    // reading first result
    readn(fd_sock, (void*)int_buf, sizeof(int));    // reads the operation result value
    if((*int_buf)!=SUCCESS) res=*int_buf;   // if the operation failed, save the error


    // ##### RETRIEVING RETURNED FILES #####

    // receives the number of files to read
    readn(fd_sock, (void*)&returned_files_num, sizeof(unsigned));     // gets the size of the file to read
    if(returned_files_num < 0) {errno=res; goto cleanup_readn;}

    // receiving the files read
    for(i=0; i<returned_files_num; i++) {
        returned_files_name_len=0;
        readn(fd_sock, (void*)&returned_files_name_len, sizeof(size_t));    // reading pathname length
        if(returned_files_name_len<0) {errno=EILSEQ; goto cleanup_readn;}
        returned_files_name=(char*)calloc(returned_files_name_len+1, sizeof(char));
        if(!returned_files_name) return ERR;   // fatal error
        memset(returned_files_name, '\0', returned_files_name_len+1);
        readn(fd_sock, (void*)returned_files_name, returned_files_name_len);     // reading pathname
        readn(fd_sock, (void*)&returned_files_size, sizeof(size_t));     // reading file size
        if(returned_files_size<0) {errno=EILSEQ; goto cleanup_readn;}
        if(returned_files_size) {
            returned_files_data=(byte*)calloc(returned_files_size, sizeof(byte));
            if(!returned_files_data) return ERR;   // fatal error
            readn(fd_sock, (void*)returned_files_data, returned_files_size);      // reading file
        }

        // if the miss dir is set, save the file in it
        if(dirname) {
            int temp_fd;
            final_path=(char*)calloc(UNIX_PATH_MAX, sizeof(char));
            if(!final_path) return ERR;  // fatal error
            memset(final_path, '\0', UNIX_PATH_MAX);
            strncat(final_path, dirname, UNIX_PATH_MAX-strlen(final_path));
            strncat(final_path, basename(returned_files_name), UNIX_PATH_MAX-strlen(final_path));
            if((temp_fd=open(final_path, O_WRONLY | O_APPEND | O_CREAT, 0777))==ERR) {
                goto cleanup_readn;
            }
            if(returned_files_size) {
                if((write(temp_fd, (void*)returned_files_data, returned_files_size))==ERR) {
                    if((close(temp_fd))==ERR) return ERR;
                    goto cleanup_readn;
                }
            }
            if((close(temp_fd))==ERR) return ERR;
            if(final_path) {free(final_path); final_path=NULL;}
        }
        // cleaning pointers
        if(returned_files_name) {free(returned_files_name); returned_files_name=NULL;}
        if(returned_files_data) {free(returned_files_data); returned_files_data=NULL;}
    }

    if(int_buf) free(int_buf);
    return returned_files_num;

// CLEANUP SECTION
cleanup_readn:
    if(int_buf) free(int_buf);
    if(returned_files_name) free(returned_files_name);
    if(returned_files_data) free(returned_files_data);
    if(final_path) free(final_path);
    if(res) errno=res;
    return ERR;
}


int writeFile(const char* pathname, const char* dirname) {
    if(!pathname) {errno=EINVAL; return ERR;}   // args cannot be NULL
    if(is_connected==0) {errno=ECONNREFUSED; return ERR;}      // the client must be connected to send requests
    errno=0;

    op_code op = WRITE_F;
    int* int_buf=(int*)malloc(sizeof(int));        // will hold certain server responses for error detection
    if(!int_buf) return ERR;
    short fd;   // will hold the file to write
    byte* buf=NULL;     // will hold the file raw content
    struct stat* fileStat=NULL;     // will hold the file info

    int i;
    int res=SUCCESS;    // will hold the result of the operation
    unsigned subst_files_num=0;     // will hold the number of files expelled from the cache
    size_t subst_files_name_len=0;    // will hold the length of the name of each of the files expelled (one at a time)
    char* subst_files_name=NULL;    // will hold the name of each of the files expelled (one at a time)
    size_t subst_files_size=0;    // will hold the size of each of the files expelled (one at a time)
    byte* subst_files_data=NULL;    // will hold the data of each of the files expelled (one at a time)
    char* final_path=NULL;  // will hold the path to save each of the the expelled file (one at a time)

    if((fd=open(pathname, O_RDONLY))==ERR) goto cleanup_write;    // opening the file
    fileStat=(struct stat*)malloc(sizeof(struct stat));     // getting file struct
    if(!fileStat) return ERR;
    if((fstat(fd, fileStat))==ERR) goto cleanup_write;  // getting file info
    size_t file_size=(size_t)fileStat->st_size;  // getting file size

    buf=(byte*)malloc(file_size*sizeof(byte));   // allocating file space
    if(!buf) return ERR;
    if((read(fd, buf, file_size))==ERR) goto cleanup_write;     // getting file's raw data into the buffer
    if((close(fd))==ERR) return ERR;


    // communicates operation
    *int_buf=op;
    writen(fd_sock, (void*)int_buf, sizeof(int));     // tells the server what operation to execute
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_write;}

    // communicates pathname lenght
    *int_buf=strlen(pathname);
    writen(fd_sock, (void*)int_buf, sizeof(int));      // tells the server what read size to expect next
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_write;}       // if the server couldn't process the size, abort the operation

    // communicates pathname
    writen(fd_sock, (void*)pathname, strlen(pathname));        // tells the server what file to write
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_write;}       // if the server could not get the path, abort the operation

    // communicates file size
    writen(fd_sock, (void*)&file_size, sizeof(size_t));      // tells the server what read size to expect next
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_write;}       // if the server couldn't process the size, abort the operation

    // sends the file data
    writen(fd_sock, (void*)buf, file_size);  // sends the actual file
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) res=*int_buf;   // if the server coudln't read the file, save the error


    // ##### RETRIEVING SUBSTITUTED FILES #####

    // receives the number of expelled files
    readn(fd_sock, (void*)&subst_files_num, sizeof(unsigned));
    if(subst_files_num<0) {errno=res; goto cleanup_write;}

    // receiving the expelled files
    for(i=0; i<subst_files_num; i++) {
        subst_files_name_len=0;
        readn(fd_sock, (void*)&subst_files_name_len, sizeof(size_t));    // reading pathname length
        if(subst_files_name_len<0) {errno=EILSEQ; goto cleanup_write;}
        subst_files_name=(char*)calloc(subst_files_name_len+1, sizeof(char));
        if(!subst_files_name) return ERR;   // fatal error
        memset(subst_files_name, '\0', subst_files_name_len+1);
        readn(fd_sock, (void*)subst_files_name, subst_files_name_len);     // reading pathname
        readn(fd_sock, (void*)&subst_files_size, sizeof(size_t));     // reading file size
        if(subst_files_size<0) {errno=EILSEQ; goto cleanup_write;}
        if(subst_files_size) {
            subst_files_data=(byte*)calloc(subst_files_size, sizeof(byte));
            if(!subst_files_data) return ERR;   // fatal error
            readn(fd_sock, (void*)subst_files_data, subst_files_size);      // reading file
        }

        // if the miss dir is set, save the file in it
        if(dirname) {
            int temp_fd;
            final_path=(char*)calloc(UNIX_PATH_MAX, sizeof(char));
            if(!final_path) return ERR;  // fatal error
            memset(final_path, '\0', UNIX_PATH_MAX);
            strncat(final_path, dirname, UNIX_PATH_MAX-strlen(final_path));
            strncat(final_path, basename(subst_files_name), UNIX_PATH_MAX-strlen(final_path));
            if((temp_fd=open(final_path, O_WRONLY | O_APPEND | O_CREAT, 0777))==ERR) {
                goto cleanup_write;
            }
            if(subst_files_size) {
                if((write(temp_fd, (void*)subst_files_data, subst_files_size))==ERR) {
                    if((close(temp_fd))==ERR) return ERR;
                    goto cleanup_write;
                }
            }
            if((close(temp_fd))==ERR) return ERR;
            if(final_path) {free(final_path); final_path=NULL;}
        }
        // cleaning pointers
        if(subst_files_name) {free(subst_files_name); subst_files_name=NULL;}
        if(subst_files_data) {free(subst_files_data); subst_files_data=NULL;}
    }


    // the write has failed, but the expelled files have been retrieved
    if(res!=SUCCESS) goto cleanup_write;

    if(int_buf) free(int_buf);
    if(buf) free(buf);
    if(fileStat) free(fileStat);
    return SUCCESS;

// CLEANUP SECTION
cleanup_write:
    if(int_buf) free(int_buf);
    if(buf) free(buf);
    if(fileStat) free(fileStat);
    if(subst_files_name) free(subst_files_name);
    if(subst_files_data) free(subst_files_data);
    if(final_path) free(final_path);
    if(res) errno=res;
    return ERR;
}


int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) {
    if(!pathname || !buf) {errno=EINVAL; return ERR;}       // pathname cannot be NULL
    if(is_connected==0) {errno=ECONNREFUSED; return ERR;}      // the client must be connected to send requests
    errno=0;

    op_code op=WRITE_F_APP;
    int* int_buf=(int*)malloc(sizeof(int));        // will hold certain server responses for error detection
    if(!int_buf) return ERR;

    int i;
    int res=SUCCESS;    // will hold the result of the operation
    unsigned subst_files_num=0;     // will hold the number of files expelled from the cache
    size_t subst_files_name_len=0;    // will hold the length of the name of each of the files expelled (one at a time)
    char* subst_files_name=NULL;    // will hold the name of each of the files expelled (one at a time)
    size_t subst_files_size=0;    // will hold the size of each of the files expelled (one at a time)
    byte* subst_files_data=NULL;    // will hold the data of each of the files expelled (one at a time)
    char* final_path=NULL;  // will hold the path to save each of the the expelled file (one at a time)


    // communicates operation
    writen(fd_sock, (void*)&op, sizeof(int));     // tells the server what operation to execute
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_append;}

    // communicates pathname lenght
    *int_buf=strlen(pathname);
    writen(fd_sock, (void*)int_buf, sizeof(int));      // tells the server what read size to expect next
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_append;}       // if the server couldn't process the size, abort the operation

    // communicates pathname
    writen(fd_sock, (void*)pathname, strlen(pathname));        // tells the server what file to write
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_append;}       // if the server could not get the path, abort the operation

    // communicates bytes to write
    writen(fd_sock, (void*)&size, sizeof(size_t));      // tells the server what read size to expect next
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_append;}       // if the server couldn't process the size, abort the operation

    // sends the file data
    writen(fd_sock, (void*)buf, size);  // sends the actual file
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) res=*int_buf;   // if the server coudln't read the file, store the err


    // ##### RETRIEVING SUBSTITUTED FILES #####

    // receives the number of expelled files
    readn(fd_sock, (void*)&subst_files_num, sizeof(unsigned));
    if(subst_files_num<0) {errno=res; goto cleanup_append;}

    // receiving the expelled files
    for(i=0; i<subst_files_num; i++) {
        subst_files_name_len=0;
        readn(fd_sock, (void*)&subst_files_name_len, sizeof(size_t));    // reading pathname length
        if(subst_files_name_len<0) {errno=EILSEQ; goto cleanup_append;}
        subst_files_name=(char*)calloc(subst_files_name_len+1, sizeof(char));
        if(!subst_files_name) return ERR;   // fatal error
        memset(subst_files_name, '\0', subst_files_name_len+1);
        readn(fd_sock, (void*)subst_files_name, subst_files_name_len);     // reading pathname
        readn(fd_sock, (void*)&subst_files_size, sizeof(size_t));     // reading file size
        if(subst_files_size<0) {errno=EILSEQ; goto cleanup_append;}
        if(subst_files_size) {
            subst_files_data=(byte*)calloc(subst_files_size, sizeof(byte));
            if(!subst_files_data) return ERR;   // fatal error
            readn(fd_sock, (void*)subst_files_data, subst_files_size);      // reading file
        }

        // if the miss dir is set, save the file in it
        if(dirname) {
            int temp_fd;
            final_path=(char*)calloc(UNIX_PATH_MAX, sizeof(char));
            if(!final_path) return ERR;  // fatal error
            memset(final_path, '\0', UNIX_PATH_MAX);
            strncat(final_path, dirname, UNIX_PATH_MAX-strlen(final_path));
            strncat(final_path, basename(subst_files_name), UNIX_PATH_MAX-strlen(final_path));
            if((temp_fd=open(final_path, O_WRONLY | O_APPEND | O_CREAT, 0777))==ERR) {
                goto cleanup_append;
            }
            if(subst_files_size) {
                if((write(temp_fd, (void*)subst_files_data, subst_files_size))==ERR) {
                    if((close(temp_fd))==ERR) return ERR;
                    goto cleanup_append;
                }
            }
            if((close(temp_fd))==ERR) return ERR;
            if(final_path) {free(final_path); final_path=NULL;}
        }
        // cleaning pointers
        if(subst_files_name) {free(subst_files_name); subst_files_name=NULL;}
        if(subst_files_data) {free(subst_files_data); subst_files_data=NULL;}
    }


    // the append has failed, but the expelled files have been retrieved
    if(res!=SUCCESS) goto cleanup_append;

    if(int_buf) free(int_buf);
    return SUCCESS;

// CLEANUP SECTION
cleanup_append:
    if(int_buf) free(int_buf);
    if(subst_files_name) free(subst_files_name);
    if(subst_files_data) free(subst_files_data);
    if(final_path) free(final_path);
    if(res) errno=res;
    return ERR;
}


int lockFile(const char* pathname) {
    if(pathname==NULL) {errno=EINVAL; return ERR;}       // pathname cannot be NULL
    if(is_connected==0) {errno=ECONNREFUSED; return ERR;}      // the client must be connected to send requests
    errno=0;

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
        writen(fd_sock, (void*)pathname, strlen(pathname));        // tells the server what file to lock
        readn(fd_sock, (void*)int_buf, sizeof(int));
        if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_lock;}       // if the server could not get the path, abort the operation

        readn(fd_sock, (void*)int_buf, sizeof(int));    // reads the operation result value
        if((*int_buf)!=SUCCESS && (*int_buf)!=EBUSY) {errno=*int_buf; goto cleanup_lock;}       // if the operation failed, return error

        if(*int_buf!=SUCCESS) {
            if((temperr=usleep(US_TO_MS(LOCK_TIMER)))==ERR)
            {LOG_ERR(errno, "sleeping while waiting for the lock"); return ERR;}
        } // sleeps before asking again for the lock
    } while((*int_buf)==EBUSY);

    if(int_buf) free(int_buf);
    return SUCCESS;

// CLEANUP SECTION
cleanup_lock:
    if(int_buf) free(int_buf);
    return ERR;
}


int unlockFile(const char* pathname) {
    if(pathname==NULL) {errno=EINVAL; return ERR;}       // pathname cannot be NULL
    if(is_connected==0) {errno=ECONNREFUSED; return ERR;}      // the client must be connected to send requests
    errno=0;

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
    writen(fd_sock, (void*)pathname, strlen(pathname));        // tells the server what file to unlock
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_unlock;}       // if the server could not get the path, abort the operation

    readn(fd_sock, (void*)int_buf, sizeof(int));    // reads the operation result value
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_unlock;}       // if the operation failed, return error


    if(int_buf) free(int_buf);
    return SUCCESS;

// CLEANUP SECTION
cleanup_unlock:
    if(int_buf) free(int_buf);
    return ERR;
}


int closeFile(const char* pathname) {
    if(pathname==NULL) {errno=EINVAL; return ERR;}       // pathname cannot be NULL
    if(is_connected==0) {errno=ECONNREFUSED; return ERR;}      // the client must be connected to send requests
    errno=0;

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
    writen(fd_sock, (void*)pathname, strlen(pathname));        // tells the server what file to close
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_close;}       // if the server could not get the path, abort the operation

    readn(fd_sock, (void*)int_buf, sizeof(int));    // reads the operation result value
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_close;}       // if the operation failed, return error


    if(int_buf) free(int_buf);
    return SUCCESS;

// CLEANUP SECTION
cleanup_close:
    if(int_buf) free(int_buf);
    return ERR;
}


int removeFile(const char* pathname) {
    if(pathname==NULL) {errno=EINVAL; return ERR;}       // pathname cannot be NULL
    if(is_connected==0) {errno=ECONNREFUSED; return ERR;}      // the client must be connected to send requests
    errno=0;

    op_code op=RM_F;        // sets the operation code to removeFile
    int* int_buf=(int*)malloc(sizeof(int));        // will hold certain server responses for error detection
    if(!int_buf) return ERR;

    // communicates operation
    *int_buf=op;
    writen(fd_sock, (void*)int_buf, sizeof(int));     // tells the server what operation to execute
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_rm;}       // if the operation prep failed for some reason, abort the operation

    // communicates pathname lenght
    *int_buf=strlen(pathname);
    writen(fd_sock, (void*)int_buf, sizeof(int));      // tells the server what read size to expect next
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_rm;}       // if the server couldn't process the size, abort the operation

    // communicates pathname
    writen(fd_sock, (void*)pathname, strlen(pathname));        // tells the server what file to remove
    readn(fd_sock, (void*)int_buf, sizeof(int));
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_rm;}       // if the server could not get the path, abort the operation

    readn(fd_sock, (void*)int_buf, sizeof(int));    // reads the operation result value
    if((*int_buf)!=SUCCESS) {errno=*int_buf; goto cleanup_rm;}       // if the operation failed, return error


    if(int_buf) free(int_buf);
    return SUCCESS;

// CLEANUP SECTION
cleanup_rm:
    if(int_buf) free(int_buf);
    return ERR;
}