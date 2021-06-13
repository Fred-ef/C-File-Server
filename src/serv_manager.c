#include "serv_manager.h"

char* sock_addr=NULL;       // will hold the server's main socket address

int main(int argc, char* argv[]) {

    if(argc==1) return -1;      // immidiately terminates if no param is passed in input
    read_config(argv[1]);       // reads the configuration to determine the server's address

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


void read_config(char* path) {
    // TODO gestire eventuali errori nella funzione di parsing
    parse_s(path, "sock_addr", &sock_addr);

    return;
}