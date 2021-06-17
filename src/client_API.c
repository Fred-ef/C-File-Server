#include "client_API.h"

short fd_sock=-1;      // initializing the socket fd to -1 (will return error if used)      
char** conn_addr=NULL;      // initializing the server-address note to NULL (will return error if used)

// Opens the connection to the server
int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    short errtemp;      // used for error-testing in syscalls
    short timeout;      // used for timeout expiration checking

    stimespec* timer=(stimespec*)malloc(sizeof(stimespec));     // implements the 200ms try-again timer
    stimespec* rem_time=(stimespec*)malloc(sizeof(stimespec));      // auxiliary structure, in case of sleep-interruption
    timer->tv_sec=0;
    timer->tv_nsec=(MS_TO_NS(RECONNECT_TIMER));     // setting the timer with the proper value

    struct sockaddr_un sa;      // server socket address

    conn_addr=(char**)malloc(sizeof(strlen(sockname)));     // memorizes the server's address in order to modify the connection later TODO memerr
    strcpy(conn_addr, sockname);        // writes the sock address in a global variable to store it
    strcpy(sa.sun_path, sockname);       // writes the sock address in the socket adress structure
    sa.sun_family = AF_UNIX;        // specifies the socket-family used (IPC communication)

    if((fd_sock=socket(AF_UNIX, SOCK_STREAM, 0))==ERR) {      // client socket creation
        errno=fd_sock;      // TODO: togliere se la socket settà già errno
        perror("Creating client socket: ");     // fatal error, print and terminate
        return ERR;
    }

    while((errtemp=connect(fd_sock, (struct sockaddr*)&sa, sizeof(sa)))==ERR && (timeout=compare_current_time(&abstime)) != -1) {     // connecting to the server socket
        if(errno==ENOENT) {
            if((errtemp=sleep_ms(RECONNECT_TIMER))==ERR) return ERR;
        }     // wait, then try again
        else {
            perror("Establishing client-server connection: ");      // fatal error, print and terminate
        return ERR;
        }
    }

    if(timeout == -1) {     // could not connect before timer expiration
        errno=ETIME;        // setting errno to a timeout error
        return ERR;
    }

    

    return SUCCESS;     // the client has successfully connected to the file server
}


int closeConnection(const char* sockname) {
    short errtemp;

    if(strcmp(sockname, conn_addr) != 0) {      // if the connection to close is different from the connection that is open, fail
        errno=EINVAL;       // setting the error number to "invalid argument"
        return ERR;
    }

    if((errtemp=close(fd_sock))==ERR) return ERR;

    return SUCCESS;     // the client has successfully closed the connection to the file server
}


static short sleep_ms(int duration) {
    short errtemp;
    stimespec* timer=(stimespec*)malloc(sizeof(stimespec));
    timer->tv_sec=0;
    timer->tv_nsec=(MS_TO_NS(duration));

    
    if((errtemp=nanosleep_w(&timer))==ERR) {
        errno=errtemp;
        return ERR;
    }

    return SUCCESS;
}