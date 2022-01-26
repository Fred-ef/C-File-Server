#include "client.h"
#include "client_API.h"

// TODO:    aggiungere punti di stampa per il flag -p

char f_flag=0;      // used in order not to duplicate the -f command
char p_flag=0;      // used in order not to duplicate the -p command
byte D_flag=0;      // used in order  to couple -D with -w or -W
byte d_flag=0;      // used in order  to couple -d with -r or -R
byte t_flag=0;      // used in order to temporally distantiate consecutive requests
byte sleep_time=0;      // used to set a sleep between consecutive requests
byte conn_timeout=10;    // used to set a time-out to connection attempts
unsigned short conn_delay=500;      // used to set a time margin between consecutive connection attempts
char* save_dir=NULL;    // used to specify the folder in which to save files retrieved from the file-server
char* miss_dir=NULL;    // used to specify the folder in which to save files discarded by the file-server


static int parse_command(char**);       // parses the command line, executing requests one by one
static void print_help();       // prints a comprehensive command list
static int is_command(char*);       // tells whether a given token is a command
static int set_sock(char*);     // sets the file-server's socket address and initiates connection
static int send_dir(char*);     // sends the specified number of files from the specified folder
static int visit_folder(DIR*, int);     // visits the dir tree rooted in the first argument
static int send_files(char*);       // sends multiple files, given as argument, to the file-server
static int set_save_dir(char*);     // specifies the directory to save files retrieved from the file-server in
static int set_miss_dir(char*);     // specifies the directory to save files discarded by the file-server in
static int read_files(char*);       // reads the files specified in the argument from the file-server
static int read_n_files(char*);     // reads the specified number of files from the file-server
static int set_time(char*);     // specifies the time the client has to wait between two consecutive requests
static int lock_files(char*);       // specifies the files to lock on the file-server
static int unlock_files(char*);     // specifies the files to unlock on the file-server
static int remove_files(char*);     // attempts to remove the specified files from the file-server



int main(int argc, char* argv[]) {

    if(argc==1) { printf("args required\n"); return ERR; }      // immidiately terminates if no param is passed in input

    unsigned short i;       // serves as an index in for-loops
    short temperr;      // used for error codes

    if((temperr=parse_command(argv))==ERR) {
        if(save_dir) free(save_dir);
        if(miss_dir) free(miss_dir);
        return ERR;
    }

    
    if(save_dir) free(save_dir);
    if(miss_dir) free(miss_dir);
    return SUCCESS;         // successful termination
}


// Parses the whole line for commands
static int parse_command(char** commands) {
    int i=1;    // loop index
    int j=1;    // secondary loop index
    int temperr;    // used for temporary error codes
    char* sockname=NULL;     // will hold the server's socket address

    // checking if the first flag is -f or -h; if not, the flag sequence is invalid (connection must be established first)
    if((strcmp(commands[i], "-f")) && (strcmp(commands[i], "-h"))) {
        errno=EINVAL;
        perror("invalid flag sequence: connection (-f) must be specified first");
        return ERR;
    }


    // ########## PARSING AND EXECUTING SETTING-OPERATIONS ##########
    while(commands[i]) {    // elaborate settings-related flags first (-h, -f, -d, -D, -p)
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
            sockname=commands[i++];   // saving the socket name for future close()
            if((temperr=set_sock(sockname))==ERR) {
                return ERR;
            }
            f_flag++;   // signaling that option '-f' has been specified
        }
        else if(!strcmp(commands[i], "-d")) {
            d_flag=1;   // signaling that option '-d' has been specified
            for(j=1; commands[j]; j++)      // searching for option '-r' or '-R' to match '-d'
                if(strcmp(commands[j], "-r") || strcmp(commands[j], "-R")) d_flag=0;
            if(d_flag) { errno=EINVAL; perror("-d must be coupled with one of -r or -R"); goto cleanup; }

            if((is_command(commands[++i]))==TRUE) {
                errno=EINVAL;
                perror("-d: argument missing");
                goto cleanup;
            }
            if((temperr=set_save_dir(commands[i++]))==ERR) {
                goto cleanup;
            }
        }
        else if(!strcmp(commands[i], "-D")) {
            D_flag=1;   // signaling that option '-D' has been specified
            for(j=1; commands[j]; j++)     // searching for option '-w' or '-W' to match '-D'
                if(strcmp(commands[j], "-w") || strcmp(commands[j], "-W")) D_flag=0;
            if(D_flag) { errno=EINVAL; perror("-D must be coupled with one of -w or -W"); goto cleanup; }

            if((is_command(commands[++i]))==TRUE) {
                errno=EINVAL;
                perror("-D: argument missing");
                goto cleanup;
            }
            if((temperr=set_miss_dir(commands[i++]))==ERR) {
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
        else if(!strcmp(commands[i], "-p")) {
            p_flag++;
            i++;
        }
        else i++;
    }
    if(!f_flag) { errno=EINVAL; perror("option -f is required"); goto cleanup; }
    if(f_flag>1 || p_flag>1) { errno=EINVAL; perror("options -f, -p, -h can only be specified once"); goto cleanup; }
    i=1;    // resetting commands index for file-operations


    // ########## PARSING AND EXECUTING FILE-OPERATIONS ##########
    while(commands[i]) {
        if(!strcmp(commands[i], "-h")) {
            i++;    // already elaborated
        }
        else if(!strcmp(commands[i], "-f")) {
            i+=2;   // already elaborated
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
            i+=2;   // already elaborated
        }
        else if(!strcmp(commands[i], "-D")) {
            i+=2;   // already elaborated
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
            if((is_command(commands[++i]))==FALSE)
                i++;    // already elaborated
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
            if((temperr=remove_files(commands[i++]))==ERR) {
                goto cleanup;
            }
        }
        else if(!strcmp(commands[i], "-p")) {
            i++;    // already elaborated
        }
        else {
            errno=EINVAL;
            perror("invalid flag");
            goto cleanup;
        }
        sleep(sleep_time);      // if -t has been used, sleep for the time specified
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
    if(!cmd) return TRUE;   // TODO revert
    if(cmd[0]=='-') return TRUE;

    return FALSE;
}


// -h | prints a list of the available commands
static void print_help() {
    // TODO
    LOG_DEBUG("Help:\n");
}


// -f | establishes the connection to the file-server
static int set_sock(char* sockname) {
    int temperr;    // used for error codes

    if(!sockname) {
        errno=EINVAL;
        perror("-f - socket address missing");
        return ERR;
    }

    stimespec* timer=(stimespec*)malloc(sizeof(stimespec));
    if(!timer) {
        perror("-f - pre-connection: memerror");
        return ERR;
    }

    if((clock_gettime(CLOCK_REALTIME, timer))==ERR) {
        perror("-f - pre-connection: getting clock time");
        if(timer) {free(timer); timer=NULL;}
        return ERR;
    }

    timer->tv_sec+=conn_timeout;

    if((temperr=openConnection(sockname, conn_delay, *timer))==ERR) {
        perror("-f - connecting");
        if(timer) {free(timer); timer=NULL;}
        return ERR;
    }

    if(timer) {free(timer); timer=NULL;}
    return SUCCESS;
}


// -w | sends n (or all) files from the specified directory to the file-server
static int send_dir(char* arg) {
    int n=0;
    int temperr;
    char* token;
    char* saveptr;
    char* dir_path=(char*)malloc(FILE_PATH_MAX*sizeof(char));
    if(!dir_path) { LOG_ERR(errno, "getting dir path"); return ERR; }


    if((token=strtok_r(arg, ",", &saveptr))==NULL) {
        LOG_ERR(EINVAL, "-w argument was invalid");
        if(dir_path) free(dir_path);
        return ERR;
    }
    strcpy(dir_path, token);     // copy the directory

    token=strtok_r(NULL, "=", &saveptr);
    if(token) token=strtok_r(NULL, "\n", &saveptr);
    if(token) n=atoi(token);
    if(n==0) n=__INT_MAX__;

    LOG_DEBUG("Dir chosen: %s;\tNumber of files: %d\n", dir_path, n);

    DIR* dir_obj;
    if(!(dir_obj=opendir(dir_path))) {
        LOG_ERR(errno, "-w - couldn't open directory");
        if(dir_path) free(dir_path);
        return ERR;
    }

    if((temperr=visit_folder(dir_obj, n))==ERR) {
        if(dir_path) free(dir_path);
        if(dir_obj) free(dir_obj);
        return ERR;
    }

    if(dir_path) free(dir_path);
    if(dir_obj) free(dir_obj);
    return SUCCESS;
}


// -W | sends the files specified by the argument string to the file-server
static int send_files(char* arg) {
    char* token;
    char* saveptr;
    int temperr;

    if((token=strtok_r(arg, ",", &saveptr))==NULL) {
        errno=EINVAL;
        perror("-W - argument was invalid");
        return ERR;
    }

    while(token) {
        if((temperr=writeFile(token, miss_dir))==ERR) {
            perror("-W - error while writing files");
            return ERR;
        }
        LOG_DEBUG("Sending file: %s\n", token);
        token=strtok_r(NULL, ",", &saveptr);
    }

    return SUCCESS;
}


// -d | specifies the folder in which to save files downloaded from the file-server
static int set_save_dir(char* dir) {
    if(!dir) {
        perror("-d - specify a valid directory");
        return ERR;
    }

    save_dir=(char*)malloc((strlen(dir)+1)*sizeof(char));
    if(!save_dir) {
        perror("-d - setting save directory");
        return ERR;
    }
    strncpy(save_dir, dir, strlen(dir)+1);

    return SUCCESS;
}


// -D | specifies the folder in which to save files discarded by the file-server
static int set_miss_dir(char* dir) {
    if(!dir) {
        perror("-D - specify a valid directory");
        return ERR;
    }

    miss_dir=(char*)malloc((strlen(dir)+1)*sizeof(char));
    if(!miss_dir) {
        perror("-D - setting save directory");
        return ERR;
    }
    strncpy(miss_dir, dir, strlen(dir)+1);

    return SUCCESS;
}


// -r | reads the files specified by the argument string from the file-server
static int read_files(char* arg) {
    int temperr;
    int fd;
    int i;
    char* token;
    char* saveptr;
    char* pathname=NULL;
    void* buffer=NULL;
    size_t* size=NULL;

    // if file storing is enabled, copy the save_dir's path in the "pathname" var
    if(save_dir) {
        // copying the save directory into a string for later elaboration
        pathname=(char*)malloc(FILE_PATH_MAX*sizeof(char));
        if(!pathname) { perror("-r - memerror"); return ERR; }
        strcpy(pathname, save_dir);
         // saving save_dir's last char position
         for(i=0; i<FILE_PATH_MAX; i++) if(pathname[i]=='\0') break;
    }

    // getting the first token
    if((token=strtok_r(arg, ",", &saveptr))==NULL) {
        errno=EINVAL;
        perror("-r - argument was invalid");
        goto cleanup_2;
    }

    // for every file in the argument string, request it to the server and save it in the save_dir if specified
    while(token) {
        if((temperr=readFile(token, &buffer, size))==ERR) {
            perror("-r - error while writing files");
            goto cleanup_2;
        }
        // if a save folder has been specified, save the file obtained there; else, discard it
        if(save_dir) {
            // if the file has been successfully retrieved, write it in the specified folder
            // else, try to retrieve next file
            if(buffer) {
                strcpy(pathname, token);    // composing the full path to save the file
                if((fd=open(token, O_CREAT, O_RDWR))==ERR) {    // creating file
                    perror("-r - creating file on disk");
                    goto cleanup_1;
                }
                if((temperr=write(fd, buffer, (*size)))==ERR) {    // writing file
                    perror("-r - writing file on disk");
                    goto cleanup_1;
                }
                if((temperr=close(fd))==ERR) {      // closing file
                    perror("-r - finalizing write on disk");
                    goto cleanup_1;
                }
                pathname[i]='\0';   // resetting save_dir's path
            }
        }
        printf("Reading file: %s\n", token);
        if(buffer) {free(buffer); buffer=NULL;}
        token=strtok_r(NULL, ",", &saveptr);
    }

    if(token) {free(token); token=NULL;}
    if(saveptr) {free(saveptr); saveptr=NULL;}
    if(pathname) {free(pathname); pathname=NULL;}
    if(buffer) {free(buffer); buffer=NULL;}
    if(size) {free(size); size=NULL;}
    return SUCCESS;

// CLEANUP
cleanup_1:
    if(size) {free(size); size=NULL;}
    if(buffer) {free(buffer); buffer=NULL;}
cleanup_2:
    if(pathname) {free(pathname); pathname=NULL;}
    return ERR;
}


// -R | reads the number specified as argument of random files from the server (all files if arg=0)
static int read_n_files(char* arg) {
    int n=0;    // default number of files
    int result;     // will hold the result of the read operation
    char* token;
    char* saveptr;

    // if the argument is valid, extract its value
    if(arg) {
        if((token=strtok_r(arg, "=", &saveptr))==NULL) {
            errno=EINVAL;
            perror("-R - invalid parameter");
            return ERR;
        }
        if(token) token=strtok_r(NULL, "\n", &saveptr);
        if(token) n=atoi(token);
    }

    // send the request to read the n files
    if((result=readNFiles(n, save_dir))==ERR) {
        perror("-R - error reading files");
        return ERR;
    }

    return result;
}


// -t | sets the time the client has to wait between two consecutive requests
static int set_time(char* arg) {
    LOG_DEBUG("Setting delay_time to %s\n", arg);
    if(!arg) return SUCCESS;
    sleep_time=atoi(arg);

    return SUCCESS;
}


// -l | locks the specified files on the file-server
static int lock_files(char* arg) {
    char* token;
    char* saveptr;
    int temperr;

    if((token=strtok_r(arg, ",", &saveptr))==NULL) {
        LOG_ERR(EINVAL, "-l - argument was invalid");
        return ERR;
    }

    while(token) {
        if((temperr=lockFile(token))==ERR) {
            LOG_ERR(errno, "-l - error while locking files");
            return ERR;
        }
        token=strtok_r(NULL, ",", &saveptr);
    }

    return SUCCESS;
}


// -u | unlocks the specified files on the file-server
static int unlock_files(char* arg) {
    char* token;
    char* saveptr;
    int temperr;

    if((token=strtok_r(arg, ",", &saveptr))==NULL) {
        LOG_ERR(EINVAL, "-u - argument was invalid");
        return ERR;
    }

    while(token) {
        if((temperr=unlockFile(token))==ERR) {
            LOG_ERR(errno, "-u - error while unlocking files");
            return ERR;
        }
        token=strtok_r(NULL, ",", &saveptr);
    }

    return SUCCESS;
}


// -c | attempts to remove the specified files from the file-server
static int remove_files(char* arg) {
    char* token;
    char* saveptr;
    int temperr;

    if((token=strtok_r(arg, ",", &saveptr))==NULL) {
        LOG_ERR(EINVAL, "-c - argument was invalid");
        return ERR;
    }

    while(token) {
        if((temperr=removeFile(token))==ERR) {
            LOG_ERR(errno, "-c - error while removing files");
            return ERR;
        }
        LOG_DEBUG("Removing file: %s\n", token);
        token=strtok_r(NULL, ",", &saveptr);
    }

    return SUCCESS;
}


// recursively visits the dir_obj folder, sending the first rem_files to the file-server (all if rem_files=0)
static int visit_folder(DIR* dir_obj, int rem_files) {
    int i;      // for loop index
    int temperr;        // used for error codes
    struct dirent* curr_file;       // holds the current file while visiting the dir tree
    DIR* next_dir;      // next dir to visit

    // allocating the string that will be used to save files' paths
    char* file_name=(char*)malloc(FILE_PATH_MAX*sizeof(char));
    if(!file_name) {LOG_ERR(errno, "-w - memerr"); return ERR;}
    // getting current dir path
    if((getcwd(file_name, FILE_PATH_MAX))==NULL) {
        LOG_ERR(errno, "-w - getting dir path");
        if(file_name) free(file_name);
        return ERR;
    }
    // completing current dir path with '/' symbol
    strncat(file_name, "/", (FILE_PATH_MAX-strlen(file_name)));
    // getting the position of the null-terminating character in current dir path
    // this will be used later to re-set the position to scan other files
    for(i=0; i<(FILE_PATH_MAX-1); i++) {
        if((file_name[i])=='\0') break;
    }

    // scan until the dir tree has been completely visited or enough files have been sent
    while(!(errno=0) && (curr_file=readdir(dir_obj)) && (rem_files>0)) {

        // avoiding to loop on the same folder or to go backwards
        if(!(strcmp(curr_file->d_name, ".")) || !(strcmp(curr_file->d_name, ".."))) continue;

        // getting the absolute path for the current file scan
        strncat(file_name, curr_file->d_name, (FILE_PATH_MAX-strlen(file_name)-strlen(curr_file->d_name)));
        LOG_DEBUG("SCANNING FILE %s\n", file_name);

        //checking file's type (dir or regular)

        // if it's a dir, visit it recursively
        if(curr_file->d_type==DT_DIR) {
            if(!(next_dir=opendir(file_name))) {
                LOG_ERR(errno, "-w - couldn't open directory");
                if(file_name) free(file_name);
                return ERR;
            }
            if((temperr=visit_folder(next_dir, rem_files))==ERR) {
                LOG_ERR(errno, "-w - switching dir");
                if(file_name) free(file_name);
                if(next_dir) free(next_dir);
                return ERR;
            }
            if(next_dir) {free(next_dir); next_dir=NULL;}
        }
        // if it's a file, send it to the file-server as a new-file
        else if(curr_file->d_type==DT_REG) {
            if((temperr=openFile(file_name, O_CREATE|O_LOCK))==ERR) {
                LOG_ERR(errno, "-w - could not send all files");
                if(errno!=EEXIST) {
                    if(file_name) free(file_name);
                    return ERR;
                }
            }
            LOG_DEBUG("FILE %s SENT\n", file_name);
            if((temperr=writeFile(file_name, save_dir))==ERR) {
                LOG_ERR(errno, "-w - could not write all files");
                if(file_name) free(file_name);
                return ERR;
            }
            rem_files--;    // decrement remaining files counter by 1
        }
        file_name[i]='\0';      // re-setting null-terminator position to current dir path
    }
    if(errno) {     // an error has occurred during the visit
        LOG_ERR(errno, "-w - error while visiting the dir tree");
        if(file_name) {free(file_name); file_name=NULL;}
    }

    if(file_name) {free(file_name); file_name=NULL;}
    return SUCCESS;
}
