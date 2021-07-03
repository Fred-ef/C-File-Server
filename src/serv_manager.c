#include "serv_manager.h"

thread_pool_cap=0;      // Indicates the maximum number of worker-threads the server can manage at the same time
sock_addr=NULL;       // will hold the server's main socket address
requests_queue=PTHREAD_MUTEX_INITIALIZER;     // Serves as a mutual exclusion mutex for worker-threads to access the requests queue


int main(int argc, char* argv[]) {

    // Initial 
    if(argc==1) exit(EXIT_FAILURE);      // immidiately terminates if no param is passed in input
    if((read_config(argv[1]))==ERR) exit(EXIT_FAILURE);       // reads the configuration to determine the server's address

    unsigned short i;       // serves as an index in for-loops
    short fd_sk;        // file descriptor for the manager's sockets
    short errtemp;      // memorizes error codes in syscalls
    struct sockaddr_un sa;      // represents the manager's socket address

    strcpy(sa.sun_path, sock_addr);       // writes the sock address in the socket adress structure
    sa.sun_family = AF_UNIX;        // specifying the socket family

    if((fd_sk=socket(AF_UNIX, SOCK_STREAM, 0))==ERR) {      // client socket creation
        errno=fd_sk;
        perror("Creating client socket: ");     // fatal error, print and terminate
        exit(EXIT_FAILURE);
    }

    if((errtemp=bind(fd_sk, (struct sockaddr*) &sa, sizeof(sa)))==ERR) {
        errno=errtemp;
        perror("Binding address to manager's socket: ");        // fatal error, print and terminate
        exit(EXIT_FAILURE);
    }

    close(fd_sk);       // closing the connection to clients
    exit(EXIT_SUCCESS);         // Successful termination
}



/* #################################################################################################### */
/* ######################################### HELPER FUNCTIONS ######################################### */
/* #################################################################################################### */


int read_config(char* path) {
    // TODO gestire eventuali errori nella funzione di parsing
    if((parse(path, "thread_pool_cap", &thread_pool_cap))==ERR) return ERR;
    if((parse_s(path, "sock_addr", &sock_addr))==ERR) return ERR;

    return SUCCESS;
}

int check_dir_path(char* path) {
    int i, temp;      // for loop index, temp var
    for(i=0; temp!='\0'; i++) {
        temp=path[i];
    }

    if((path[0]!='/') && (path[0]!='.') && (path[i]!='/')) return ERR;

    return SUCCESS;
}