#include "client.h"

char* sock_addr=NULL;        // will hold the server's main socket's address

int main(int argc, char* argv[]) {

    if(argc==1) return -1;      // immidiately terminates if no param is passed in input
    read_config(argv[1]);       // reads the configuration to determine the server's address

    unsigned short i;       // serves as an index in for-loops
    short fd_sk;
    short errtemp;
    struct sockaddr_un sa;
    
    strcpy(sa.sun_path, sock_addr);       // writes the sock address in the socket adress structure
    sa.sun_family = AF_UNIX;

    if((fd_sk=socket(AF_UNIX, SOCK_STREAM, 0))==ERR) {      // client socket creation
        errno=fd_sk;
        perror("Creating client socket: ");     // fatal error, print and terminate
        exit(EXIT_FAILURE);
    }

    while(connect(fd_sk, (struct sockaddr*)&sa, sizeof(sa))==ERR) {     // connecting to the server socket
        if(errno==ENOENT) sleep(1);     // wait, then try again
        perror("Establishing client-server connection: ");      // fatal error, print and terminate
        exit(EXIT_FAILURE);
    }



    close(fd_sk);       // closing the connection towards the server
    exit(EXIT_SUCCESS);         // successful termination
}


void read_config(char* path) {
    // TODO gestire eventuali errori nella funzione di parsing
    parse_s(path, "sock_addr", &sock_addr);     // getting the server's socket address from the config file

    return;
}