#include "client.h"
#include "client_API.h"

char f_flag=0;      // used in order not to duplicate the -f command
char p_flag=0;      // used in order not to duplicate the -p command
byte D_flag=0;      // used in order  to couple -D with -w or -W
byte d_flag=0;      // used in order  to couple -d with -r or -R
byte t_flag=0;      // used in order to temporally distantiate consecutive requests
byte sleep_time=0;      // used to set a sleep between consecutive requests
byte conn_timeout=10;    // used to set a time-out to connection attempts
unsigned conn_delay=500;      // used to set a time margin between consecutive connection attempts

llist* open_files_list=NULL;      // used to keep track of the files opened by the client


static int parse_command(char**);       // parses the command line, executing requests one by one
static void print_help();       // prints a comprehensive command list
static int is_command(char*);       // tells whether a given token is a command
static int set_sock(char*);     // sets the file-server's socket address and initiates connection
static int send_dir(char*);     // sends the specified number of files from the specified folder
static int visit_folder(char*, int*);     // visits the dir tree rooted in the first argument
static int send_files(char*);       // sends multiple files, given as argument, to the file-server
static int set_save_dir(char*);     // specifies the directory to save files retrieved from the file-server in
static int set_miss_dir(char*);     // specifies the directory to save files discarded by the file-server in
static int read_files(char*);       // reads the files specified in the argument from the file-server
static int read_n_files(char*);     // reads the specified number of files from the file-server
static int set_time(char*);     // specifies the time the client has to wait between two consecutive requests
static int lock_files(char*);       // specifies the files to lock on the file-server
static int unlock_files(char*);     // specifies the files to unlock on the file-server
static int remove_files(char*);     // attempts to remove the specified files from the file-server
static int append_files(char*);     // writes the file passed as argument in append
static int close_files(llist*);    // closes all files saved in the queue
static int str_ptr_cmp(const void*, const void*);   // used to compare void pointers as strings



int main(int argc, char* argv[]) {

    if(argc==1) { LOG_ERR(EINVAL, "args required\n"); return ERR; }      // immidiately terminates if no param is passed in input

    short temperr;      // used for error codes

    open_files_list=ll_create();    // creating the list to keep track of the open files
    if(!open_files_list) {LOG_ERR(errno, "creating open files list"); return ERR;}

    if((temperr=parse_command(argv))==ERR) {
        if(save_dir) free(save_dir);
        if(miss_dir) free(miss_dir);
        return ERR;
    }

    if((temperr=ll_dealloc_full(open_files_list))==ERR)
    {LOG_ERR(errno, "destroying open files list"); return ERR;}


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
        LOG_ERR(EINVAL, "invalid flag sequence: connection (-f) must be specified first");
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
                LOG_ERR(EINVAL, "-f - argument missig");
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
            if(d_flag) { LOG_ERR(EINVAL, "-d must be coupled with one of -r or -R"); goto cleanup; }

            if((is_command(commands[++i]))==TRUE) {
                LOG_ERR(EINVAL, "-d - argument missing");
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
            if(D_flag) { LOG_ERR(EINVAL, "-D must be coupled with one of -w or -W"); goto cleanup; }

            if((is_command(commands[++i]))==TRUE) {
                LOG_ERR(EINVAL, "-D - argument missing");
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
    if(!f_flag) { LOG_ERR(EINVAL, "option -f is required"); goto cleanup; }
    if(f_flag>1 || p_flag>1) { LOG_ERR(EINVAL, "options -f, -p, -h can only be specified once"); goto cleanup; }
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
                LOG_ERR(EINVAL, "-w: argument missing");
                goto cleanup;
            }
            if((temperr=send_dir(commands[i++]))==ERR) {
                goto cleanup;
            }
        }
        else if(!strcmp(commands[i], "-W")) {
            if((is_command(commands[++i]))==TRUE) {
                LOG_ERR(EINVAL, "-W: argument missing");
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
                LOG_ERR(EINVAL, "-r: argument missing");
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
                LOG_ERR(EINVAL, "-l: argument missing");
                goto cleanup;
            }
            if((temperr=lock_files(commands[i++]))==ERR) {
                goto cleanup;
            }
        }
        else if(!strcmp(commands[i], "-u")) {
            if((is_command(commands[++i]))==TRUE) {
                LOG_ERR(EINVAL, "-u: argument missing");
                goto cleanup;
            }
            if((temperr=unlock_files(commands[i++]))==ERR) {
                goto cleanup;
            }
        }
        else if(!strcmp(commands[i], "-c")) {
            if((is_command(commands[++i]))==TRUE) {
                LOG_ERR(EINVAL, "-c: argument missing");
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
            LOG_ERR(EINVAL, "invalid flag");
            goto cleanup;
        }
        if((usleep(US_TO_MS(sleep_time)))==ERR)      // if -t has been used, sleep for the time specified
        {LOG_ERR(errno, "sleeping between operations"); return ERR;}
    }

    if((temperr=close_files(open_files_list))==ERR) return ERR;
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
        LOG_ERR(EINVAL, "-f - socket address missing");
        return ERR;
    }

    stimespec* timer=(stimespec*)malloc(sizeof(stimespec));
    if(!timer) {
        LOG_ERR(errno, "-f - pre-connection: memerr");
        return ERR;
    }

    if((clock_gettime(CLOCK_REALTIME, timer))==ERR) {
        LOG_ERR(errno, "-f - pre-connection: getting clock time");
        if(timer) {free(timer); timer=NULL;}
        return ERR;
    }

    timer->tv_sec+=conn_timeout;

    if((temperr=openConnection(sockname, conn_delay, *timer))==ERR) {
        LOG_ERR(errno, "-f - connecting");
        if(timer) {free(timer); timer=NULL;}
        return ERR;
    }
    if(timer) {free(timer); timer=NULL;}

    
    LOG_OUTPUT("-f: connected successfully\n");
    return SUCCESS;
}


// -w | sends n (or all) files from the specified directory to the file-server
static int send_dir(char* arg) {
    int n=0;
    int temperr;
    char* token;
    char* saveptr;
    char* dir_path=(char*)malloc(UNIX_PATH_MAX*sizeof(char));
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

    if((temperr=visit_folder(dir_path, &n))==ERR) {
        if(dir_path) free(dir_path);
        return ERR;
    }

    if(dir_path) free(dir_path);
    return SUCCESS;
}


// -W | sends the files specified by the argument string to the file-server
static int send_files(char* arg) {
    char* token;
    char* saveptr;
    int temperr;

    if((token=strtok_r(arg, ",", &saveptr))==NULL) {
        LOG_ERR(EINVAL, "-W - argument was invalid");
        return ERR;
    }

    while(token) {
        // if the file doesn't yet exist one the server, creates it
        if((temperr=openFile(token, O_CREATE|O_LOCK))==SUCCESS
            || (temperr==ERR && errno==0)) {    // non-fatal error, can continue
            // after successful creation, writes the file
            if((temperr=writeFile(token, miss_dir))==ERR) {
                LOG_ERR(errno, "-W - could not write all files");
                return ERR;
            }
        }
        // after successful creation, writes the file
        else {
            // if it did exist, just open it and write in append
            if(errno==EEXIST) {
                if((temperr=openFile(token, 0))==ERR) {    // opening the file
                    LOG_ERR(errno, "-W - could not open file");
                    return ERR;
                }
                if((temperr=append_files(token))==ERR) {
                    LOG_ERR(errno, "-W - could not write in append");
                    return ERR;
                }
            }
            // fatal error
            else {
                LOG_ERR(errno, "-W - could not create all files");
                return ERR;
            }
        }
        // pushing the opened file's name into the open file queue
        char* filename=strdup(token);
        if(!filename) {LOG_ERR(errno, "-W - could not register file opening"); return ERR;}
        if((temperr=ll_insert_head(open_files_list, (void*)filename, str_ptr_cmp))==ERR)
        {LOG_ERR(errno, "-W: pushing filename into open files list"); return ERR;}

        struct stat* file_stat=(struct stat*)malloc(sizeof(struct stat));
        if(!file_stat) return ERR;
        if((temperr=stat(filename, file_stat))==ERR)
        {LOG_ERR(errno, "-W getting file info"); return ERR;}
        LOG_OUTPUT("-W: successfully wrote file  \"%s\" to the server (%lubytes written)\n",token, file_stat->st_size);
        free(file_stat);

        token=strtok_r(NULL, ",", &saveptr);
    }

    return SUCCESS;
}


// -d | specifies the folder in which to save files downloaded from the file-server
static int set_save_dir(char* dir) {
    if(!dir) {
        LOG_ERR(errno, "-d - specify a valid directory");
        return ERR;
    }

    save_dir=(char*)malloc((strlen(dir)+1)*sizeof(char));
    if(!save_dir) {
        LOG_ERR(errno, "-d - setting save directory");
        return ERR;
    }
    strncpy(save_dir, dir, strlen(dir)+1);


    LOG_OUTPUT("Save folder for returned files: %s\n", dir);
    return SUCCESS;
}


// -D | specifies the folder in which to save files discarded by the file-server
static int set_miss_dir(char* dir) {
    if(!dir) {
        LOG_ERR(errno, "-D - specify a valid directory");
        return ERR;
    }

    miss_dir=(char*)malloc((strlen(dir)+1)*sizeof(char));
    if(!miss_dir) {
        LOG_ERR(errno, "-D - setting save directory");
        return ERR;
    }
    strncpy(miss_dir, dir, strlen(dir)+1);


    LOG_OUTPUT("Save folder for expelled files: %s\n", dir);
    return SUCCESS;
}


// -r | reads the files specified by the argument string from the file-server
static int read_files(char* arg) {
    int temperr;
    int fd;
    int i;
    char* token=NULL;
    char* saveptr=NULL;
    char* pathname=NULL;
    void* buffer=NULL;
    size_t size=0;

    // if file storing is enabled, copy the save_dir's path in the "pathname" var
    if(save_dir) {
        // copying the save directory into a string for later elaboration
        pathname=(char*)malloc(UNIX_PATH_MAX*sizeof(char));
        if(!pathname) { LOG_ERR(errno, "-r - memerror"); return ERR; }
        memset(pathname, '\0', UNIX_PATH_MAX);
        strcpy(pathname, save_dir);
         // saving save_dir's last char position
         for(i=0; i<UNIX_PATH_MAX; i++) if(pathname[i]=='\0') break;
    }

    // getting the first token
    if((token=strtok_r(arg, ",", &saveptr))==NULL) {
        LOG_ERR(EINVAL, "-r - argument was invalid");
        goto cleanup_2;
    }

    // for every file in the argument string, request it to the server and save it in the save_dir if specified
    while(token) {
        if((temperr=openFile(token, 0))==ERR) {    // opening the file
            LOG_ERR(errno, "-r - could not open file");
            return ERR;
        }

        // pushing the opened file's name into the open file queue
        char* filename=strdup(token);
        if(!filename) {LOG_ERR(errno, "-r - could not register file opening"); return ERR;}
        if((temperr=ll_insert_head(open_files_list, (void*)filename, str_ptr_cmp))==ERR)
        {LOG_ERR(errno, "-r: pushing filename into open files list"); return ERR;}

        if((temperr=readFile(token, &buffer, &size))==ERR) {    // reading the file
            LOG_ERR(errno, "-r - error while reading files");
            goto cleanup_2;
        }
        LOG_OUTPUT("-r: successfully read file \"%s\" from the server (%lubytes read\n", (char*)token, size);
        // if a save folder has been specified, save the file obtained there; else, discard it
        if(save_dir) {
            // if the file has been successfully retrieved, write it in the specified folder
            // else, try to retrieve next file
            if(buffer) {
                strncat(pathname, basename(token), UNIX_PATH_MAX-strlen(pathname)-1); 
                if((fd=open(pathname, O_WRONLY | O_APPEND | O_CREAT, 0777))==ERR) {    // creating file
                    LOG_ERR(errno, "-r - creating file on disk");
                    goto cleanup_1;
                }
                if((temperr=write(fd, buffer, size))==ERR) {    // writing file
                    LOG_ERR(errno, "-r - writing file on disk");
                    goto cleanup_1;
                }
                if((temperr=close(fd))==ERR) {      // closing file
                    LOG_ERR(errno, "-r - closing file");
                    goto cleanup_1;
                }
                pathname[i]='\0';   // resetting save_dir's path
            }
        }
        if(buffer) {free(buffer); buffer=NULL;}
        token=strtok_r(NULL, ",", &saveptr);
    }

    if(pathname) {free(pathname); pathname=NULL;}
    if(buffer) {free(buffer); buffer=NULL;}
    return SUCCESS;

// CLEANUP
cleanup_1:
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
            LOG_ERR(EINVAL, "-R - invalid parameter");
            return ERR;
        }
        if(token) token=strtok_r(NULL, "\n", &saveptr);
        if(token) n=atoi(token);
    }

    // send the request to read the n files
    if((result=readNFiles(n, save_dir))==ERR) {
        LOG_ERR(errno, "-R - error reading files");
        return ERR;
    }


    LOG_OUTPUT("-R: successfully read %d files from the server\n", result);
    return result;
}


// -t | sets the time the client has to wait between two consecutive requests
static int set_time(char* arg) {
    if(!arg) return SUCCESS;
    sleep_time=atoi(arg);


    LOG_OUTPUT("Timer set: %dms\n", sleep_time);
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
        if((temperr=openFile(token, 0))==ERR) {    // opening the file
            LOG_ERR(errno, "-l - could not open file");
            return ERR;
        }

        // pushing the opened file's name into the open file queue
        char* filename=strdup(token);
        if(!filename) {LOG_ERR(errno, "-l - could not register file opening"); return ERR;}
        if((temperr=ll_insert_head(open_files_list, (void*)filename, str_ptr_cmp))==ERR)
        {LOG_ERR(errno, "-l: pushing filename into open files list"); return ERR;}

        if((temperr=lockFile(token))==ERR) {    // locking the file
            LOG_ERR(errno, "-l - error while locking files");
            return ERR;
        }
        LOG_OUTPUT("-l: successfully locked file \"%s\"\n", token);
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
        LOG_OUTPUT("-u: successfully unlocked file \"%s\"\n", token);
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
        LOG_OUTPUT("-c: successfully removed file \"%s\" from the server\n", token);
        token=strtok_r(NULL, ",", &saveptr);
    }

    return SUCCESS;
}


// recursively visits the starting_dir folder, sending the first rem_files to the file-server (all if rem_files=0)
static int visit_folder(char* starting_dir, int* rem_files) {
    if(!starting_dir || !rem_files) {LOG_ERR(EINVAL, "-w - null arguments"); return ERR;}

    int temperr;        // used for error codes
    char* curr_elem_name=NULL;   // current visited element name
    struct dirent* curr_elem_p=NULL;       // holds the current element structure while visiting the dir tree
    DIR* curr_dir_p=NULL;      // current working directory structure


    // allocating the string that will be used to save files' paths
    curr_elem_name=(char*)calloc(UNIX_PATH_MAX, sizeof(char));
    if(!curr_elem_name) {LOG_ERR(errno, "-w - memerr"); return ERR;}
    memset((void*)curr_elem_name, '\0', UNIX_PATH_MAX*sizeof(char));

    // copying the current dir path as the starting of the files' paths and completing with '/'
    strcpy(curr_elem_name, starting_dir);
    strncat(curr_elem_name, "/", (UNIX_PATH_MAX-strlen(curr_elem_name)));

    // getting the position of the null-terminating character in current dir path
    // this will be used later to re-set the position to scan other files
    int string_ter=strlen(curr_elem_name);

    if(!(curr_dir_p=opendir(starting_dir))) {
        LOG_ERR(errno, "-w - couldn't open directory");
        goto cleanup_vis_fold;
    }

    // scan until the current dir has been completely visited or enough files have been sent
    while(!(errno=0) && (curr_elem_p=readdir(curr_dir_p)) && (*rem_files>0)) {

        // avoiding to loop on the same folder or going backwards
        if(!(strcmp(curr_elem_p->d_name, ".")) || !(strcmp(curr_elem_p->d_name, ".."))) continue;

        // getting the absolute path for the current file scan
        strncat(curr_elem_name, curr_elem_p->d_name, (UNIX_PATH_MAX-strlen(curr_elem_name)-strlen(curr_elem_p->d_name)));

        //checking file's type (dir or regular)

        // if it's a dir, visit it recursively
        if(curr_elem_p->d_type==DT_DIR) {
            if((temperr=visit_folder(curr_elem_name, rem_files))==ERR) {
                LOG_ERR(errno, "-w - starting new dir visit");
                goto cleanup_vis_fold;
            }
        }
        // if it's a file, send it to the file-server as a new-file or append it if it already exists
        else if(curr_elem_p->d_type==DT_REG) {
            // if the file doesn't yet exist one the server, creates it
            if((temperr=openFile(curr_elem_name, O_CREATE|O_LOCK))==SUCCESS
                || (temperr==ERR && errno==0)) {    // non-fatal error, can continue
                // after successful creation, writes the file
                if((temperr=writeFile(curr_elem_name, miss_dir))==ERR) {
                    LOG_ERR(errno, "-w - could not write all files");
                    goto cleanup_vis_fold;
                }
            }
            // after successful creation, writes the file
            else {
                // if it did exist, just open it and write in append
                if(errno==EEXIST) {
                    if((temperr=openFile(curr_elem_name, 0))==ERR) {    // opening the file
                        LOG_ERR(errno, "-w - could not open file");
                        goto cleanup_vis_fold;
                    }
                    if((temperr=append_files(curr_elem_name))==ERR) {
                        LOG_ERR(errno, "-w - could not write in append");
                        goto cleanup_vis_fold;
                    }
                }
                // fatal error
                else {
                    LOG_ERR(errno, "-w - could not create all files");
                    goto cleanup_vis_fold;
                }
            }
            // pushing the opened file's name into the open file list
            char* filename=strdup(curr_elem_name);
            if(!filename) {LOG_ERR(errno, "-w - could not register file opening"); return ERR;}
            if((temperr=ll_insert_head(open_files_list, (void*)filename, str_ptr_cmp))==ERR)
            {LOG_ERR(errno, "-w: pushing filename into open files list"); return ERR;}

            struct stat* file_stat=(struct stat*)malloc(sizeof(struct stat));
            if(!file_stat) return ERR;
            if((temperr=stat(filename, file_stat))==ERR)
            {LOG_ERR(errno, "-w getting file info"); return ERR;}
            LOG_OUTPUT("-w: successfully wrote file  \"%s\" to the server (%lubytes written)\n",curr_elem_name, file_stat->st_size);
            free(file_stat);
            
            (*rem_files)--;    // decrement remaining files counter by 1
        }
        curr_elem_name[string_ter]='\0';      // re-setting null-terminator position to current dir path
    }

    if(errno) {     // an error has occurred during the visit
        LOG_ERR(errno, "-w - error while visiting the dir tree");
        goto cleanup_vis_fold;
    }

    // when the visit ends, the dir must be closed
    if((temperr=closedir(curr_dir_p))==ERR) {
        LOG_ERR(errno, "-w - couldn't open directory");
        goto cleanup_vis_fold;
    }


    if(curr_elem_name) {free(curr_elem_name); curr_elem_name=NULL;}
    return SUCCESS;

// CLEANUP SECTION
cleanup_vis_fold:
    if(curr_elem_name) {free(curr_elem_name); curr_elem_name=NULL;}
    return ERR;
}


// writes the file in append to the server
static int append_files(char* arg) {
    int temperr;
    int fd;
    struct stat* file_stat=NULL;     // will hold file information
    size_t file_size;     // will hold file name
    byte* buf=NULL;      // will hold file data

    if((fd=open(arg, O_RDONLY))==ERR) {errno=EINVAL; return ERR;}
    file_stat=(struct stat*)malloc(sizeof(struct stat));     // getting file struct
    if(!file_stat) return ERR;
    if((fstat(fd, file_stat))==ERR) return ERR;  // getting file info
    file_size=(size_t)file_stat->st_size;  // getting file size
    buf=(byte*)calloc(file_size, sizeof(byte));
    if(!buf) return ERR;
    if((read(fd, buf, file_size))==ERR) return ERR;     // getting file's raw data into the buffer
    if((close(fd))==ERR) return ERR;

    if((temperr=appendToFile(arg, buf, file_size, miss_dir))==ERR) return ERR;


    if(file_stat) free(file_stat);
    if(buf) free(buf);
    return SUCCESS;
}


// closes every file whose name is stored in the queue
static int close_files(llist* open_files) {
    if(!open_files) {LOG_ERR(EINVAL, "closing files: open files list uninitialized"); return ERR;}
    int temperr;
    char* temp_file;

    // if the list is empty, there is nothing to close
    if(!(ll_isEmpty(open_files))) {
        conc_node aux1=open_files->head;
        while ((aux1!=NULL)) {
            temp_file=(char*)(aux1->data);
            if((temperr=closeFile(temp_file))==ERR && errno!=ENOENT) {
                LOG_ERR(errno, "closing all opened files");
                return ERR;
            }
            aux1=aux1->next;
        }
    }


    return SUCCESS;
}


// helper function: compares two void pointers as strings
static int str_ptr_cmp(const void* s, const void* t) {
    // warning: segfault if one of the pointers is null
    return strcmp((char*)s, (char*)t);
}
