#include "client.h"
#include "client_API.h"

// TODO:    decidere se aggiungere config file e, in caso, scrivere funzione per parsarlo

char* sock_addr=NULL;        // will hold the server's main socket's address

int main(int argc, char* argv[]) {

    if(argc==1) return -1;      // immidiately terminates if no param is passed in input

    unsigned short i;       // serves as an index in for-loops
    short temperr;      // used for error codes

    if((temperr=parse_command(argv))==ERR) return -1;

    
    return 0;         // successful termination
}


// Parses the whole line for commands
static int parse_command(char** commands) {
    int i=1;  // loop index
    int temperr;    // used for temporary error codes
    char* sockname=NULL;     // will hold the server's socket address

    // checking if the first flag is -f or -h; if not, the flag sequence is invalid (connection must be established first)
    if((strcmp(commands[i], "-f")) && (strcmp(commands[i], "-h"))) {
        errno=EINVAL;
        perror("invalid flag sequence: connection (-f) must be specified first");
        return ERR;
    }

    // ########## PARSING AND EXECUTING THE CURRENT COMMAND ##########
    while(commands[i]) {
        if(!strcmp(commands[i], "-h")) {
            print_help();
            return SUCCESS;     // this command immediately terminates the client
        }
        else if(!strcmp(commands[i], "-f")) {
            if((is_command(commands[++i]))==TRUE) {
                errno=EINVAL;
                perror("-f: argument missing");
                return ERR;
            }
            sockname=commands[i];   // saving the socket name for future close()
            if((temperr=set_sock(sockname))==ERR) {
                return ERR;
            }
        }
        else if(!strcmp(commands[i], "-w")) {
            if((is_command(commands[++i]))==TRUE) {
                errno=EINVAL;
                perror("-w: argument missing");
                goto cleanup;
            }
            if((temperr=send_dir(commands[i++]))==ERR) {
                goto cleanup;
            }
        }
        else if(!strcmp(commands[i], "-W")) {
            if((is_command(commands[++i]))==TRUE) {
                errno=EINVAL;
                perror("-W: argument missing");
                goto cleanup;
            }
            if((temperr=send_files(commands[i++]))==ERR) {
                goto cleanup;
            }
        }
        else if(!strcmp(commands[i], "-d")) {
            if((is_command(commands[++i]))==TRUE) {
                errno=EINVAL;
                perror("-d: argument missing");
                goto cleanup;
            }
            if((temperr=set_dir_read(commands[i++]))==ERR) {
                goto cleanup;
            }
        }
        else if(!strcmp(commands[i], "-D")) {
            if((is_command(commands[++i]))==TRUE) {
                errno=EINVAL;
                perror("-D: argument missing");
                goto cleanup;
            }
            if((temperr=set_dir_miss(commands[i++]))==ERR) {
                goto cleanup;
            }
        }
        else if(!strcmp(commands[i], "-r")) {
            if((is_command(commands[++i]))==TRUE) {
                errno=EINVAL;
                perror("-r: argument missing");
                goto cleanup;
            }
            if((temperr=read_files(commands[i++]))==ERR) {
                goto cleanup;
            }
        }
        else if(!strcmp(commands[i], "-R")) {
            if((is_command(commands[++i]))==TRUE) {
                    if((temperr=read_n_files(NULL))==ERR) {
                    goto cleanup;
                }
            }
            else if((temperr=read_n_files(commands[i++]))==ERR) {
                goto cleanup;
            }
        }
        else if(!strcmp(commands[i], "-t")) {
            if((is_command(commands[++i]))==TRUE) {
                    if((temperr=set_time(NULL))==ERR) {
                    goto cleanup;
                }
            }
            else if((temperr=set_time(commands[i++]))==ERR) {
                goto cleanup;
            }
        }
        else if(!strcmp(commands[i], "-l")) {
            if((is_command(commands[++i]))==TRUE) {
                errno=EINVAL;
                perror("-l: argument missing");
                goto cleanup;
            }
            if((temperr=lock_files(commands[i++]))==ERR) {
                goto cleanup;
            }
        }
        else if(!strcmp(commands[i], "-u")) {
            if((is_command(commands[++i]))==TRUE) {
                errno=EINVAL;
                perror("-u: argument missing");
                goto cleanup;
            }
            if((temperr=unlock_files(commands[i++]))==ERR) {
                goto cleanup;
            }
        }
        else if(!strcmp(commands[i], "-c")) {
            if((is_command(commands[++i]))==TRUE) {
                errno=EINVAL;
                perror("-c: argument missing");
                goto cleanup;
            }
            if((temperr=delete_files(commands[i++]))==ERR) {
                goto cleanup;
            }
        }
        else if(!strcmp(commands[i], "-p")) {
            print_output();
            i++;
            continue;
        }
        else {
            errno=EINVAL;
            perror("invalid flag");
            goto cleanup;
        }
    }

    // if either -d or -D have been used but not coupled with -w/-W or -r/-R, return an error
    if((d_flag>0) || (D_flag>0)) {
        errno=EINVAL;
        perror("-d/-D cannot be solo-flags");
        goto cleanup;
    }

    closeConnection(sockname);
    return SUCCESS;

    // ERROR CLEANUP SECTION
cleanup:
    closeConnection(sockname);
    return ERR;
}


// checks if the given token is a command flag, whether it is valid or not
static int is_command(char* cmd) {
    if(!cmd) return FALSE;
    if(cmd[0]=='-') return TRUE;

    return FALSE;
}


// prints a list of the available commands
static void print_help() {
    // TODO
}


// establishes the connection to the file-server
static int set_sock(char* sockname) {
    int temperr;    // used for error codes

    if(!sockname) {
        errno=EINVAL;
        perror("socket address missing");
        return ERR;
    }

    stimespec* timer=(stimespec*)malloc(sizeof(stimespec));
    if(!timer) {
        perror("pre-connection: memerror");
        return ERR;
    }

    if((clock_gettime(CLOCK_REALTIME, timer))==ERR) {
        perror("pre-connection: getting clock time");
        free(timer);
        return ERR;
    }

    timer->tv_sec+=conn_timeout;

    if((temperr=openConnection(sockname, conn_delay, *timer))==ERR) {
        perror("connecting");
        free(timer);
        return ERR;
    }

    free(timer);
    return SUCCESS;
}


// sends n (or all) files from the specified directory to the file-server
static int send_dir()