#include "client_API.h"             

short fd_sock=-1;      // initializing the socket fd to -1 (will return error if used)
char* conn_addr=NULL;      // initializing the server-address note to NULL (will return error if used)
static bool is_connected=0;     // set to 1 when the connection with the server is established, 0 otherwise

// Opens the connection to the server
int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    short errtemp;      // used for error-testing in syscalls
    short timeout;      // used for timeout expiration checking

    stimespec* timer=NULL;       // implements the 200ms try-again timer
    stimespec* rem_time=NULL;        // auxiliary structure, in case of sleep-interruption

    timer=(stimespec*)malloc(sizeof(stimespec));
    if(!timer) {errno=ENOMEM; goto cleanup;}

    rem_time=(stimespec*)malloc(sizeof(stimespec));      
    if(!rem_time) {errno=ENOMEM; goto cleanup;}

    timer->tv_sec=0;        // no need for seconds!
    timer->tv_nsec=(MS_TO_NS(RECONNECT_TIMER));     // setting the timer with the proper value

    struct sockaddr_un sa;      // server socket address

    conn_addr=calloc(strlen(sockname), sizeof(char));       // memorizes the server's address in order to modify the connection later TODO memerr
    if(!conn_addr) {errno=ENOMEM; goto cleanup;}

    strcpy(conn_addr, sockname);        // writes the sock address in a global variable to store it
    strcpy(sa.sun_path, sockname);       // writes the sock address in the socket adress structure
    sa.sun_family = AF_UNIX;        // specifies the socket-family used (IPC communication)

    if((fd_sock=socket(AF_UNIX, SOCK_STREAM, 0))==ERR) goto cleanup;

    while((errtemp=connect(fd_sock, (struct sockaddr*)&sa, sizeof(sa)))==ERR && (timeout=compare_current_time(&abstime)) != -1) {     // connecting to the server socket
        if(errno==ENOENT) {
            if((errtemp=sleep_ms(RECONNECT_TIMER))==ERR) goto cleanup;
        }     // wait, then try again
        else goto cleanup;
    }

    if(timeout == -1) {errno=ETIME; goto cleanup;}      // could not connect before timer expiration

    is_connected=1;
    cleanup_pointers((void*)timer, (void*)rem_time, (void*)conn_addr, NULL);        // cleaning all the allocated memory
    return SUCCESS;     // the client has successfully connected to the file server

cleanup:        // executes in case something went wrong and the operation cannot be successfully completed
    cleanup_pointers((void*)timer, (void*)rem_time, (void*)conn_addr, NULL);        // cleaning all the allocated memory
    return ERR;     // informs the caller that the operation failed
}


int closeConnection(const char* sockname) {
    short errtemp;

    if(strcmp(sockname, conn_addr) != 0) {      // if the connection to close is different from the connection that is open, fail
        errno=EINVAL;       // setting the error number to "invalid argument"
        return ERR;
    }

    if((errtemp=close(fd_sock))==ERR) return ERR;       // couldn't close socket

    return SUCCESS;     // the client has successfully closed the connection to the file server
}


// TODO error management & errno
int openFile(const char* pathname, int flags) {
    if(pathname==NULL) {errno=EINVAL; return ERR;}       // pathname cannot be NULL
    if(is_connected==0) {errno=ECONNREFUSED; return ERR;}      // the client must be connected to send requests

    int server_ret_code;        // will hold certain server responses for error detection
    op_code op=OPEN_F;        // sets the operation code to openFile

    // communicates operation
    writen(fd_sock, (void*)op, sizeof(op));     // tells the server what operation to execute
    readn(fd_sock, (void*)server_ret_code, sizeof(int));        // receives the server's reply to the operation
    if(server_ret_code!=SUCCESS) {errno=server_ret_code; return ERR;}       // if the operation prep failed for some reason, abort the operation

    // communicates pathname lenght
    writen(fd_sock, (void*)sizeof(pathname), sizeof(int));      // tells the server what read size to expect next
    readn(fd_sock, (void*)server_ret_code, sizeof(int));
    if(server_ret_code!=SUCCESS) {errno=server_ret_code; return ERR;}       // if the server couldn't process the size, abort the operation

    // communicates file content
    writen(fd_sock, (void*)pathname, sizeof(pathname));        // tells the server what file to open
    readn(fd_sock, (void*)server_ret_code, sizeof(int));
    if(server_ret_code!=SUCCESS) {errno=server_ret_code; return ERR;}       // if the server could not open the file, abort the operation

    // communicates flags
    writen(fd_sock, (void*)flags, sizeof(int));     // tells the server what flags to use in the open
    readn(fd_sock, (void*)server_ret_code, sizeof(int));
    if(server_ret_code!=SUCCESS) {errno=server_ret_code; return ERR;}       // if the flags are invalid, abort the operation

    return SUCCESS;       // the file has been opened on the server
}


// TODO error management & errno
int readFile(const char* pathname, void** buf, size_t* size) {
    if(pathname==NULL) {errno=EINVAL; return ERR;}       // pathname cannot be NULL
    if(is_connected==0) {errno=ECONNREFUSED; return ERR;}      // the client must be connected to send requests

    int server_ret_code;        // will hold certain server responses for error detection
    op_code op=READ_F;        // sets the operation code to openFile

    writen(fd_sock, (void*)op, sizeof(op));     // tells the server what operation to execute
    readn(fd_sock, (void*)server_ret_code, sizeof(int));        // receives the server's reply to the operation
    if(server_ret_code!=SUCCESS) {errno=server_ret_code; return ERR;}       // if the operation prep failed for some reason, abort the operation

    writen(fd_sock, (void*)sizeof(pathname), sizeof(int));      // tells the server what read size to expect next
    readn(fd_sock, (void*)server_ret_code, sizeof(int));
    if(server_ret_code!=SUCCESS) {errno=server_ret_code; return ERR;}       // if the server couldn't process the size, abort the operation

    writen(fd_sock, (void*)pathname, sizeof(pathname));        // tells the server what file to send
    readn(fd_sock, (void*)server_ret_code, sizeof(int));
    if(server_ret_code!=SUCCESS) {errno=server_ret_code; return ERR;}       // if the server could not open the file, abort the operation

    read(fd_sock, (void*)(*size), sizeof(int));     // gets the size of the file to read
    if(*size <= 0) 

    return SUCCESS;
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

    
    if((errtemp=nanosleep_w(&timer))==ERR) {
        return ERR;
    }

    return SUCCESS;
}