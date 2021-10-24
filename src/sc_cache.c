#include "sc_cache.h"

nth_chance=2;      // indicates the "chance" order of the algorithm (2 for second chance)

static int open_file(file* file, int* usr_id);
static int read_file(file* file_to_read, byte** data_read, int* usr_id);
static int lock_file(file* file, int* usr_id);
static int unlock_file(file* file, int* usr_id);
static int remove_file(file* file, int* usr_id);
static int close_file(file* file, int* usr_id);
static int write_file(file* file, byte* data_written, unsigned bytes_written, int* usr_id);
static int write_append(file* file, byte* data_written, unsigned bytes_written, int* usr_id);
static int is_duplicate(file* file1, file* file2);
static int int_ptr_cmp(const void* x, const void* y);

// TODO -   aggiornare spazio cache nelle write
// TODO -   de-allocare lo spazio quando la modifica fallisce
// TODO -   controllare che si liberi la cache
// TODO -   controllare che si liberino le code di file aperti
// TODO -   controllare che si liberi la copia dei file letti
// TODO -   aggiungere cleanup per pulire l'intera cache dai file
char** strings=NULL;


sc_cache* sc_cache_create(int max_file_number, int max_byte_size) {
    if(!max_file_number || !max_byte_size)
        {LOG_ERR(EINVAL, "create: cache cannot have size=0"); goto cleanup_create;}
    
    // allocating cache memory
    sc_cache* cache=(sc_cache*)malloc(sizeof(sc_cache));
    if(!cache) {LOG_ERR(errno, "create: allocating Cache memory"); goto cleanup_create;}
    

    // allocating cache's hash table memory
    cache->ht=conc_hash_create(max_file_number);
    if(!(cache->ht)) {LOG_ERR(errno, "create: allocating Cache's Hashtable"); goto cleanup_create;}

    cache->max_file_number=max_file_number;     // set the maximum numbers of file allowed
    cache->max_byte_size=max_byte_size;         // set the maximum memory usable
    cache->curr_file_number=0;      // the cache is initially empty
    cache->curr_byte_size=0;        // the cache is initially empty

    // initializing cache's memory-related mutex
    int temperr=pthread_mutex_init(&(cache->mem_check_mtx), NULL);
    if(temperr) {LOG_ERR(errno, "create: setting Cache's size-mutex"); goto cleanup_create;}
    
    return cache;

// CLEANUP SECTION:
cleanup_create:
    if(cache && cache->ht) free(cache->ht);
    if(cache) free(cache);
    return NULL;
}


int sc_cache_insert(sc_cache* cache, file* new_file, file*** replaced_files) {
    if(!cache || !new_file || !replaced_files) {LOG_ERR(EINVAL, "insert: cache_push args cannot be NULL"); return ERR;}
    if(!(cache->ht)) {LOG_ERR(EINVAL, "insert: cache's hashtable not initialized");}

    int temperr;    // used for error codes
    short res=PROBING;    // termination condition
    conc_hash_table* ht=cache->ht;      // shortcut to the hashtable



    // checking if there's enough space in the cache to memorize the file
    temperr=pthread_mutex_lock(&(cache->mem_check_mtx));
    if(temperr) {LOG_ERR(temperr, "insert: locking cache's mem-mutex"); return ERR;}

    // if the file is too large for the cache, return error
    if(new_file->file_size > cache->max_byte_size) {LOG_ERR(EFBIG, "file is too large"); return ERR;}
    // if there isn't enough space, call the replacement algorithm
    if(((cache->curr_file_number + 1) > cache->max_file_number)
    || ((cache->curr_byte_size + new_file->file_size) > cache->max_byte_size)) {
        
        temperr=pthread_mutex_unlock(&(cache->mem_check_mtx));      // unlocking cache's mem-mtx
        if(temperr) {LOG_ERR(temperr, "insert: unlocking cache's mem-mutex"); return ERR;}

        // invoking the replacement algorithm
        if((temperr=sc_algorithm(cache, new_file->file_size, replaced_files, TRUE))==ERR)
            return ERR;     // the caller will handle the error and eventually try again (errno is set)
    }
    else {  // else, pre-allocate the element
        cache->curr_file_number++;      // incrementinc chache's file number
        cache->curr_byte_size+=(new_file->file_size);       // incrementing cache's size

        temperr=pthread_mutex_unlock(&(cache->mem_check_mtx));      // unlocking cache's mem-mtx
        if(temperr) {LOG_ERR(temperr, "insert: unlocking cache's mem-mutex"); return ERR;}
    }

    
    
    // getting the starting index for our file
    unsigned int idx=conc_hash_hashfun((new_file->name), (ht->size));
    while(res==PROBING) {       // finding a free entry for the file
        // locking the hashtable entry
        temperr=pthread_mutex_lock(&(((ht->table)[idx]).entry_mtx));
        if(temperr) {LOG_ERR(temperr, "insert: locking hashtable's entry"); return ERR;}

        // cheking if the entry is free
        if(((ht->table)[idx]).entry==NULL || ((ht->table)[idx]).entry==(ht->mark)) {

            ((ht->table)[idx]).entry=(void*)new_file;   // saving the file in the current entry
            ((ht->table)[idx]).r_used=1;    // setting the used-bit

            LOG_DEBUG("PUSHED CORRECTLY\n\n#FILE: %d\t#BYTES: %d\n", (cache->curr_file_number), (cache->curr_byte_size));     // TODO REMOVE

            res=FOUND;      // exiting the while-loop
        }

        // if the element is not free and it is a duplicate, return an error
        // TODO gestire meglio, che casino (togliere unlock e metterne una alla fine)
        else if((temperr=is_duplicate(((ht->table)[idx]).entry, new_file))) {  // TODO implementare
            LOG_DEBUG("THREAD: %d\tKEY %d WAS DUPLICATE!!!\n", (gettid()), idx);
            res=EEXIST;
        }

        // else, continue probing
        temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));      // unlocking the hashtable entry
        if(temperr) {LOG_ERR(temperr, "insert: unlocking hashtable's entry"); return ERR;}

        idx++;
        idx=idx%(ht->size*2);
    }   // HASH SECTION END


    // if the insertion failed, return an error
    if(res!=FOUND) goto cleanup_insert;
    LOG_DEBUG("INSERTED IN LIST\n");       // TODO REMOVE
    return res;     // returns the insert operation's result

// CLEANUP SECTION
cleanup_insert:
// de-allocating the pre-allocated memory
    temperr=pthread_mutex_lock(&(cache->mem_check_mtx));
    if(temperr) {LOG_ERR(temperr, "insert: locking cache's mem-mutex"); return ERR;}
    cache->curr_file_number--;
    cache->curr_byte_size-=new_file->file_size;
    temperr=pthread_mutex_unlock(&(cache->mem_check_mtx));
    if(temperr) {LOG_ERR(temperr, "insert: unlocking cache's mem-mutex"); return ERR;}
    errno=res;
    return ERR;
}


int sc_algorithm(sc_cache* cache, unsigned size_var, file*** replaced_files, bool is_insert) {
    if(!cache) {LOG_ERR(EINVAL, "replace: cache not initialized"); return ERR;}

    int i, j;          // loop indexes
    int k=0;          // holds the number of replaced files
    int exit=0;       // used to exit the procedure's main loops in case of success
    int temperr;    // used for code errors
    conc_hash_table* ht=cache->ht;      // hashtable shortcut

    (*replaced_files)=(file**)calloc(cache->max_file_number, sizeof(file*));
    if(!(*replaced_files)) {LOG_ERR(errno, "replace: allocating space for replaced files"); return ERR;}
    for(i=0; i<cache->max_file_number; i++) { (*replaced_files)[i]=NULL;}   // setting all pointers to NULL

    // cycling through every entry, for 2 times at max
    for(i=0; i<nth_chance && !exit; i++) {       // perform twice if the first visit was unsuccessful

        for(j=0; j<((cache->ht->size)*2) && !exit; j++) {    // scan the whole hashtable, searching for files to replace
            // locking current entry
            temperr=pthread_mutex_lock(&(((ht->table)[j]).entry_mtx));
            if(temperr) {LOG_ERR(temperr, "replace: locking hashtable's entry"); return ERR;}

            if((ht->table)[j].entry && (ht->table)[j].entry!=ht->mark) {
                if((ht->table)[j].r_used) (ht->table)[j].r_used=0;  // if the item was recently used, reset its used-bit
                else {
                    // if the file isn't currently locked, proceed to its elimination
                    if(!((file*)ht->table[j].entry)->f_lock) {
                        // locking cache's mem-mtx
                        temperr=pthread_mutex_lock(&(cache->mem_check_mtx));
                        if(temperr) {LOG_ERR(temperr, "replace: locking cache's mem-mutex"); return ERR;}

                        (*replaced_files)[k++]=(file*)ht->table[j].entry;     // getting a reference to the file removed  TODO creare array e aggiungere RETURN
                        cache->curr_file_number--;
                        cache->curr_byte_size-=((file*)ht->table[j].entry)->file_size;
                        ht->table[j].entry=ht->mark;        // marking the entry as "deleted"
                        
                        // checking if the file removal freed enough space
                        if((cache->curr_file_number <= cache->max_file_number)
                        && ((cache->curr_byte_size + size_var) <= cache->max_byte_size)) {
                            // if the operation that caused the replacement was an insertion, up the file counter
                            if(is_insert) cache->curr_file_number++;
                            cache->curr_byte_size+=size_var;    // add the size variation specified to the memory used
                            exit=1;     // exiting the loops
                        }

                        // unlocking cache's mem-mtx
                        temperr=pthread_mutex_unlock(&(cache->mem_check_mtx));
                        if(temperr) {LOG_ERR(temperr, "replace: unlocking cache's mem-mutex"); return ERR;}
                    }
                }
            }
            // unlocking current entry
            temperr=pthread_mutex_unlock(&(((ht->table)[j]).entry_mtx));
            if(temperr) {LOG_ERR(temperr, "replace: unlocking hashtable's entry"); return ERR;}
        }
    }


    // if the whole cache has been scanned twice unsuccessfully, return an error
    if(!exit) {errno=ENOSPC; return ERR;}   // if the algorithm couldn't free enough space, return an error
    
    return SUCCESS;

// CLEANUP SECTION
cleanup_alg:
    return ERR;
}


int sc_lookup(sc_cache* cache, char* file_name, op_code op, int* usr_id, byte** data_read, byte* data_written, unsigned bytes_written) {
    // ########## CONTROL SECTION ##########
    if(!cache) {LOG_ERR(EINVAL, "lookup: cache is uninitialized"); return ERR;}
    if(!file_name) {LOG_ERR(EINVAL, "lookup: invalid file name specified"); return ERR;}
    if(!usr_id) {LOG_ERR(EINVAL, "lookup: the usr id given is invalid"); return ERR;}
    if(op<1 || op>10) {LOG_ERR(EINVAL, "lookup: invalid operation code"); return ERR;}
    if(op==READ_F && !data_read) {LOG_ERR(EINVAL, "lookup: invalid pointer provided"); return ERR;}
    if((op==WRITE_F || op==WRITE_F_APP) && !data_written) {LOG_ERR(EINVAL, "lookup: invalid pointer provided"); return ERR;} 
    if((op==WRITE_F || op==WRITE_F_APP) && !bytes_written) {LOG_ERR(EINVAL, "lookup: invalid write size"); return ERR;}

    // ########## USEFUL DECLARATIONS ##########
    int temperr;    // used for error codes
    short res=PROBING;    // termination condition (in case you don't like break;)
    conc_hash_table* ht=cache->ht;      // shortcut to the hashtable
    file* temp_file=NULL;       // auxiliary pointer


    // ########## MAIN OPERATION ##########
    // getting the starting index for the lookup operation
    unsigned int idx=conc_hash_hashfun((file_name), (ht->size));
    while(res==PROBING) {
        // locking the hashtable entry
        temperr=pthread_mutex_lock(&(((ht->table)[idx]).entry_mtx));
        if(temperr) {LOG_ERR(temperr, "lookup: locking hashtable's entry"); return ERR;}

        // if the entry is NULL, the element is not in the hashtable
        if(((ht->table)[idx]).entry==NULL) res=ENOENT;

        // if there's a match, perform the correct operation base on the op_code
        else if(!strcmp(file_name, ((file*)ht->table[idx].entry)->name)) {
            temp_file=(file*)ht->table[idx].entry;  // shortcut to current file
            ht->table[idx].r_used=1;    // since the file has just been referred, update its used-bit

            // Calls a different function based on the operation flag
            if(op==OPEN_F) {    // opens the file if it's not locked
                res=open_file(temp_file, usr_id);
                if(res==ERR) {LOG_ERR(errno, "lookup: opening file"); return ERR;}   // a fatal error (memerr/mutexerr) has occurred
            }

            else if(op==OPEN_C_F) res=ENOENT;    // checking this means the file already exists, so return error

            else if(op==READ_F) {   // if file open returns its data, else returns error
                res=read_file(temp_file, data_read, usr_id);
                if(res==ERR) {LOG_ERR(errno, "lookup: reading file"); return ERR;}  // a fatal error (memerr/mutexerr) has occurred
                ht->table[idx].entry=ht->mark;  // the item has been deleted, so mark it as deleted
            }

            else if(op==WRITE_F) {  // if last operation on the file was create&lock, writes the file's content
                res=write_file(temp_file, data_written, bytes_written, usr_id);
                if(res==ERR) {LOG_ERR(errno, "lookup: writing file"); return ERR;}  // a fatal error (memerr/mutexerr) has occurred
            }

            else if(op==WRITE_F_APP) {  // if the file isn't locked by another user, writes data in append
                res=write_append(temp_file, data_written, bytes_written, usr_id);
                if(res==ERR) {LOG_ERR(errno, "lookup: writing file in append"); return ERR;}    // a fatal error (memerr/mutexerr) has occurred
            }

            else if(op==LOCK_F) {   // attempts to lock the file
                res=lock_file(temp_file, usr_id);   // if file is unlocked, locks it
                if(res==ERR) {LOG_ERR(errno, "lookup: locking file"); return ERR;}  // a fatal error (memerr/mutexerr) has occurred
            }

            else if(op==UNLOCK_F) res=unlock_file(temp_file, usr_id);    // if file is locked by the user, unlocks it
            
            else if(op==RM_F) {     // if the file is locked by the user, deletes it from the cache
                res=remove_file(temp_file, usr_id);        // if file is locked by the user, deletes it
                if(res==ERR) {LOG_ERR(errno, "lookup: removing file"); return ERR;} // a fatal error (memerr/mutexerr) has occurred
            }

            else if(op==CLOSE_F) {  // closes the file for the user
                res=close_file(temp_file, usr_id);     // closes file for the user
                if(res==ERR) {LOG_ERR(errno, "lookup: closing file"); return ERR;}  // a fatal error (memerr/mutexerr) has occurred
            }
        }
        
        // else, continue probing
        temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));
        if(temperr) {LOG_ERR(temperr, "lookup: unlocking hashtable's entry"); return ERR;}

        idx++;
        idx=idx%(ht->size*2);
    }


    return res;     // returns the result of the operation performed
}


// helper function: opens the file for the user; if it was already opened, returns successfully
static int open_file(file* file, int* usr_id) {
    int temperr;

    int* real_id=(int*)malloc(sizeof(int));
    if(!real_id) return ERR;    // fatal error, errno alreadyset by the call
    *real_id=*usr_id;

    if((temperr=ll_insert_head(file->open_list, (void*)real_id, int_ptr_cmp))==ERR)  // insert usr_id in the file_open list
    return ERR;   // fatal error, errno already set by the call

    return SUCCESS;
}


// helper function: returns a copy of the file's data if previously opened
static int read_file(file* file_to_read, byte** data_read, int* usr_id) {
    int i;  // index for loop
    int res;

    if(file_to_read->f_lock && (file_to_read->f_lock)!=(*usr_id)) return EBUSY;    // if file is locked by another user, return error
    res=ll_search(file_to_read->open_list, (void*)usr_id, int_ptr_cmp);     // checking if the file has been opened by the user
    if(!res) return EPERM;      // file not opened: operation not permitted
    if(res==ERR) return ERR;    // fatal error, errno already set by the call

    // file was opened by the user
    if(file_to_read->file_size) {   // if the file has any data, copies it into the buffer passed, allocating it first
        (*data_read)=(byte*)malloc((file_to_read->file_size)*sizeof(byte));
        if(!(*data_read)) return ERR;  // fatal error, errno already set by the call

        // copying data from file to buffer
        // TODO cambiare con la versione DECOMPRESSA
        for(i=0; i<(file_to_read->file_size); i++) (*data_read)[i]=(file_to_read->data)[i];
    }
    else (*data_read)=NULL;     // file was empty

    return SUCCESS;
}


// helper function: if file is unlocked, locks the file; if user has the lock, returns successfully
static int lock_file(file* file, int* usr_id) {
    int res;

    if(file->f_lock && (file->f_lock)!=(*usr_id)) return EBUSY;    // lock is held by another user
    if(file->f_lock && (file->f_lock)==(*usr_id)) return SUCCESS;  // user already has the lock

    res=ll_search(file->open_list, (void*)usr_id, int_ptr_cmp);     // checking if the file has been opened by the user
    if(!res) return EPERM;      // file not opened: operation not permitted
    if(res==ERR) return ERR;    // fatal error, errno already set by the call

    file->f_lock=(*usr_id);    // locks the file for the user
    return SUCCESS;
}


// helper function: if the lock is held by the user, releases it; else, just returns successfully
static int unlock_file(file* file, int* usr_id) {
    if(file->f_lock && (file->f_lock!=(*usr_id))) return SUCCESS;  // user wasn't holding any lock

    file->f_lock=0;     // resets lock
    return SUCCESS;
}


// helper function: if the user has a lock on the file, deletes it from the server
static int remove_file(file* file, int* usr_id) {
    if(file->f_lock != (*usr_id)) return EPERM;    // the user doesn't have a lock on the file; operation not permitted

    int temperr;

    if(file->data) free(file->data);
    if(file->name) free(file->name);
    if(file->open_list) {
        temperr=ll_dealloc_full(file->open_list);
        if(temperr==ERR) return ERR;    // fatal error, errno already set by the call
    }
    free(file);

    return SUCCESS;
}


// helper function: closes the file for the user; if it wasn't opened by the user, returns successfully
static int close_file(file* file, int* usr_id) {
    int temperr;

    if(file->f_lock==(*usr_id)) file->f_lock=0;    // if the user has a lock on the file, unlock it before closing it
    if((temperr=ll_remove(file->open_list, (void*)usr_id, int_ptr_cmp))==ERR)  // insert usr_id in the file_open list
    return ERR;   // fatal error, errno already set by the call

    return SUCCESS;
}


// helper function: writes the whole file if the last operation on it was create&lock
static int write_file(file* file, byte* data_written, unsigned bytes_written, int* usr_id) {
    if(file->data || file->f_lock!=(*usr_id)) return EPERM;    // last operation was not create&lock

    int i;  // for loop index

    file->data=(byte*)malloc(bytes_written*sizeof(byte));
    if(!file->data) {LOG_ERR(errno, "writing file: memerr"); return ERR;}
    file->file_size=bytes_written;  // updating file size

    for(i=0; i<bytes_written; i++) {    // writing data into the file
        (file->data)[i] = data_written[i];      // TODO CAMBIARE CON LA COMPRESSIONE
    }

    return SUCCESS;
}


// helper function: writes the data passed as arg in append mode
static int write_append(file* file, byte* data_written, unsigned bytes_written, int* usr_id) {
    if(file->f_lock && (file->f_lock)!=(*usr_id)) return EPERM;    // another user has a lock on this file

    int i=0, j;  // for loop index
    int res;

    res=ll_search(file->open_list, (void*)usr_id, int_ptr_cmp);     // checking if the file has been opened by the user
    if(!res) return EPERM;      // file not opened: operation not permitted
    if(res==ERR) return ERR;    // fatal error, errno already set by the call

    unsigned write_size=file->file_size+bytes_written;
    byte* updated_data=(byte*)malloc(write_size*sizeof(byte));
    if(!updated_data) return ERR;   // fatal error, errno already set by the call

    // if the file already has some data in it, copy it to the updated version
    if(file->file_size) for(i=0; i<(file->file_size); i++) updated_data[i]=(file->data)[i];
    for(j=0; i<write_size && j<bytes_written; i++, j++) updated_data[i]=data_written[j];

    if(file->data) free(file->data);    // destroying old data
    file->data=updated_data;    // updating file's data
    file->file_size=write_size;     // updating file's size

    return SUCCESS;
}


// helper function: returns TRUE if the files' names are equal, FALSE otherwise
static int is_duplicate(file* file1, file* file2) {
    // NULL checks already done in the insert procedure
    if(!strcmp(file1->name, file2->name)) return TRUE;
    else return FALSE;
}


// helper function: compares two void pointers as integers
static int int_ptr_cmp(const void* x, const void* y) {
    if((*(int*)x)==(*(int*)y)) return 0;    // x=y
    else if((*(int*)x)>(*(int*)y)) return 1;    // x>y
    else return -1;      // x<y
}







/*
void rand_str(char *dest, size_t length) {
    char charset[] = "0123456789"
                     "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';
}

void* pushfun(void* arg) {
    sc_cache* cache=(sc_cache*)arg;
    int i;
    printf("Thread %d starting\n", (gettid()));
    for(i=0; i<50; i++) {
        byte* num=(byte*)malloc(sizeof(byte));
        *num=i;
        file** replaced_files;
        file* file=lru_cache_build_file(strings[i], i+1, num, 0, 0, 0);
        sc_cache_insert(cache, file, &replaced_files);
        printf("Thread %d pushed\n", (gettid()));
    }

    return (void*) NULL;
}

void* popfun(void* arg) {
    sc_cache* cache=(sc_cache*)arg;
    int i;
    file* file;
    printf("Thread popper %d starting\n", (gettid()));
    for(i=0; i<50; i++) {
        file=sc_lookup(cache, file, RM_F, &i, NULL, NULL, 0);
        if(file)printf("Thread %d popped file '%s'\n", (gettid()), file->name);
    }

    return (void*)NULL;
}

void* lookupfun(void* arg) {
    lru_cache* cache=(lru_cache*)arg;
    int i;
    file* file;
    printf("Thread looker %d starting\n", (gettid()));
    for(i=0; i<50; i++) {
        file=lru_cache_lookup(cache, strings[i]);
        if(file) printf("#################### I got file: '%s'\n", (file->name));
    }

    return (void*)NULL;
}

void* removefun(void* arg) {
    lru_cache* cache=(lru_cache*)arg;
    int i;
    file* file;
    printf("Thread remover %d starting\n", (gettid()));
    for(i=0; i<50; i++) {
        file=lru_cache_remove(cache, strings[i]);
        if(file) printf("#################### I got file: '%s'\n", (file->name));
    }

    return (void*)NULL;
}

int main() {
    lru_cache* cache=lru_cache_create(500, 10000);
    if(!cache) return -1;
    pthread_t* tid_arr=(pthread_t*)malloc(20*sizeof(pthread_t));
    if(!tid_arr) return -1;
    int i;

    strings=(char**)malloc(50*sizeof(char*));
    for(i=0; i<50; i++) {
        char str[] = { [41] = '\1' }; // make the last character non-zero so we can test based on it later
        rand_str(str, sizeof str - 1);
        strings[i]=(char*)malloc(255*sizeof(char));
        strcpy(strings[i], str);
    }


    for(i=0; i<20; i++) {
        if((i%2)==0) {
            pthread_create(&(tid_arr[i]), NULL, pushfun, (void*)cache);
            printf("Spawned thread PUSHER %d\n", i);
        }
        else {
            pthread_create(&(tid_arr[i]), NULL, lookupfun, (void*)cache);
            printf("Spawned thread POPPER %d\n", i);
        }
    }

    for(i=0; i<20; i++) {
        pthread_join(tid_arr[i], NULL);
        printf("Joined thread %d\n", i);
    }
    

    for(i=0; i<10; i++) {
        pthread_create(&(tid_arr[i]), NULL, pushfun, (void*)cache);
        printf("Spawned thread PUSHER %d\n", i);
    }

    for(i=0; i<10; i++) {
        pthread_join(tid_arr[i], NULL);
        printf("Joined thread PUSHER %d\n", i);
    }

    for(i=0; i<10; i++) {
        pthread_create(&(tid_arr[i]), NULL, removefun, (void*)cache);
        printf("Spawned thread REMOVER %d\n", i);
    }

    for(i=0; i<10; i++) {
        pthread_join(tid_arr[i], NULL);
        printf("Joined thread REMOVER %d\n", i);
    }


    printf("#files: %d\t#bytes: %d\n", cache->curr_file_number, cache->curr_byte_size);

    return 0;
}
*/