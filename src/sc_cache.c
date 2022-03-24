#include "sc_cache.h"

byte nth_chance=2;      // indicates the "chance" order of the algorithm (2 for second chance)

unsigned long subst_file_num=0;       // indicates how many files have been replaced during the execution
unsigned long max_file_number_reached=0;      // indicates the maximum number of file that have been stored at the same time
unsigned long max_byte_size_reached=0;        // indicates the maximum number of bytes that have been stored at the same time

static int open_file(file* file, const int* usr_id);
static int read_file(file* file_to_read, byte** data_read, size_t* bytes_used, const int* usr_id);
static int lock_file(file* file, const int* usr_id);
static int unlock_file(file* file, const int* usr_id);
static int remove_file(file* file, const int* usr_id);
static int close_file(file* file, const int* usr_id);
static int write_file(file* file, const byte* data_written, const size_t* bytes_used, const int* usr_id);
static int write_append(file* file, const byte* data_written, const size_t* bytes_used, const int* usr_id);
static int is_duplicate(const file* file1, const file* file2);
static int int_ptr_cmp(const void* x, const void* y);


sc_cache* sc_cache_create(const int max_file_number, const int max_byte_size) {
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


int sc_cache_insert(sc_cache* cache, const file* new_file, file*** replaced_files) {
    if(!cache || !new_file || !replaced_files) {LOG_ERR(EINVAL, "insert: cache_push args cannot be NULL"); return ERR;}
    if(!(cache->ht)) {LOG_ERR(EINVAL, "insert: cache's hashtable not initialized"); return ERR;}

    int temperr;    // used for error codes
    short res=PROBING;    // termination condition
    conc_hash_table* ht=cache->ht;      // shortcut to the hashtable



    // checking if there's enough space in the cache to memorize the file
    temperr=pthread_mutex_lock(&(cache->mem_check_mtx));
    if(temperr) {LOG_ERR(temperr, "insert: locking cache's mem-mutex"); return ERR;}

    // if the file is too large for the cache, return error
    if(new_file->file_size > cache->max_byte_size) {LOG_ERR(EFBIG, "file is too large"); res=EFBIG;}
    // if there isn't enough space, call the replacement algorithm
    else if(((cache->curr_file_number + 1) > cache->max_file_number)
    || ((cache->curr_byte_size + new_file->file_size) > cache->max_byte_size)) {
        
        temperr=pthread_mutex_unlock(&(cache->mem_check_mtx));      // unlocking cache's mem-mtx
        if(temperr) {LOG_ERR(temperr, "insert: unlocking cache's mem-mutex"); return ERR;}

        // invoking the replacement algorithm
        if((temperr=sc_algorithm(cache, new_file->file_size, replaced_files, TRUE, new_file->name))!=SUCCESS) {
                if(errno!=ENOSPC) return ERR;   // fatal error
                res=ENOSPC;
            }
    }
    else {  // else, pre-allocate the element
        cache->curr_file_number++;      // incrementinc chache's file number
        if((cache->curr_file_number)>max_file_number_reached) max_file_number_reached=cache->curr_file_number;
        cache->curr_byte_size+=(new_file->file_size);       // incrementing cache's size
        if((cache->curr_byte_size)>max_byte_size_reached) max_byte_size_reached=cache->curr_byte_size;

        temperr=pthread_mutex_unlock(&(cache->mem_check_mtx));      // unlocking cache's mem-mtx
        if(temperr) {LOG_ERR(temperr, "insert: unlocking cache's mem-mutex"); return ERR;}
    }
    
    
    // getting the starting index for the file
    unsigned int idx=conc_hash_hashfun((new_file->name), (ht->size));
    while(res==PROBING) {       // finding a free entry for the file
        // locking the hashtable entry
        temperr=pthread_mutex_lock(&(((ht->table)[idx]).entry_mtx));
        if(temperr) {LOG_ERR(temperr, "insert: locking hashtable's entry"); return ERR;}

        // cheking if the entry is free
        if(((ht->table)[idx]).entry==NULL || ((ht->table)[idx]).entry==(ht->mark)) {

            ((ht->table)[idx]).entry=(void*)new_file;   // saving the file in the current entry
            ((ht->table)[idx]).r_used=nth_chance-1;    // setting the used-bit
            res=FOUND;      // exiting the while-loop
        }

        // if the element is not free and it is a duplicate, return an error
        else if((temperr=is_duplicate(((ht->table)[idx]).entry, new_file))) {
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
    return SUCCESS;     // returns the insert operation's result

// CLEANUP SECTION
cleanup_insert:
// de-allocating the pre-allocated memory
    if(res!=EFBIG && res!=ENOSPC) {
        temperr=pthread_mutex_lock(&(cache->mem_check_mtx));
        if(temperr) {LOG_ERR(temperr, "insert: locking cache's mem-mutex"); return ERR;}
        cache->curr_file_number--;
        cache->curr_byte_size-=new_file->file_size;
        temperr=pthread_mutex_unlock(&(cache->mem_check_mtx));
        if(temperr) {LOG_ERR(temperr, "insert: unlocking cache's mem-mutex"); return ERR;}
    }
    errno=res;
    return ERR;
}


int sc_algorithm(sc_cache* cache, const size_t size_var, file*** replaced_files, const bool is_insert, const char* filename) {
    if(!cache) {LOG_ERR(EINVAL, "replace: cache not initialized"); return ERR;}
    if(!replaced_files) {LOG_ERR(EINVAL, "expelled files' array not initialized"); return EINVAL;}

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

            // checking if the file (still) exists and if it's the same file we're trying to write/insert
            if((ht->table)[j].entry && (ht->table)[j].entry!=ht->mark && (strcmp(((file*)ht->table[j].entry)->name, filename))) {
                if((ht->table)[j].r_used) (ht->table)[j].r_used--;  // if the item was recently used, reset its used-bit
                else {
                    // if the file isn't currently locked nor opened, proceed to its elimination
                    if((!((file*)ht->table[j].entry)->f_lock)) {
                        // locking cache's mem-mtx
                        temperr=pthread_mutex_lock(&(cache->mem_check_mtx));
                        if(temperr) {LOG_ERR(temperr, "replace: locking cache's mem-mutex"); return ERR;}

                        (*replaced_files)[k++]=(file*)ht->table[j].entry;     // getting a reference to the file removed
                        cache->curr_file_number--;
                        cache->curr_byte_size-=((file*)ht->table[j].entry)->file_size;
                        ht->table[j].entry=ht->mark;        // marking the entry as "deleted"
                        
                        // checking if the file removal freed enough space
                        if((cache->curr_file_number <= cache->max_file_number)
                        && ((cache->curr_byte_size + size_var) <= cache->max_byte_size)) {
                            // if the operation that caused the replacement was an insertion, up the file counter
                            if(is_insert) cache->curr_file_number++;
                            cache->curr_byte_size+=size_var;    // add the size variation specified to the memory used
                            if((cache->curr_byte_size)>max_byte_size_reached) max_byte_size_reached=cache->curr_byte_size;
                            exit=1;     // exiting the loops
                        }
                        subst_file_num++;   // incrementing the number of substituted files

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
}


// lookup procedure: wraps every possible operation but "insert" and "readNFiles"
int sc_lookup(sc_cache* cache, const char* file_name, const op_code op, const int* usr_id, byte** data_read, const byte* data_written, size_t* bytes_used, file*** replaced_files) {
    // ########## CONTROL SECTION ##########
    if(!cache) {LOG_ERR(EINVAL, "lookup: cache is uninitialized"); return ERR;}
    if(!file_name) {LOG_ERR(EINVAL, "lookup: invalid file name specified"); return ERR;}
    if(!usr_id) {LOG_ERR(EINVAL, "lookup: the usr id given is invalid"); return ERR;}
    if(op<1 || op>10) {LOG_ERR(EINVAL, "lookup: invalid operation code"); return ERR;}
    if(op==READ_F && !data_read) {LOG_ERR(EINVAL, "lookup: invalid data pointer provided"); return ERR;}
    if(op==READ_F && !bytes_used) {LOG_ERR(EINVAL, "lookup: invalid size pointer provided"); return ERR;}
    if((op==WRITE_F || op==WRITE_F_APP) && !data_written) {LOG_ERR(EINVAL, "lookup: invalid pointer provided"); return ERR;} 
    if((op==WRITE_F || op==WRITE_F_APP) && !bytes_used) {LOG_ERR(EINVAL, "lookup: invalid write size"); return ERR;}


    // ########## USEFUL DECLARATIONS ##########
    int temperr;    // used for error codes
    short res=PROBING;    // termination condition (in case you don't like break;)
    conc_hash_table* ht=cache->ht;      // shortcut to the hashtable
    file* temp_file=NULL;       // auxiliary pointer


    // ########## RESERVING MEMORY FOR WRITE OPERATIONS ##########
    if(op==WRITE_F || op==WRITE_F_APP) {
        // pre-allocating the memory needed to perform the write operation, returning eventually removed files

        // checking if there's enough space in the cache to memorize the file
        temperr=pthread_mutex_lock(&(cache->mem_check_mtx));
        if(temperr) {LOG_ERR(temperr, "lookup-write: locking cache's mem-mutex"); return ERR;}

        // if the file is too large for the cache, return error
        if((*bytes_used) > cache->max_byte_size) {LOG_ERR(EFBIG, "file is too large"); res=EFBIG;}
        
        // if there isn't enough space, call the replacement algorithm
        else if((cache->curr_byte_size + (*bytes_used)) > cache->max_byte_size) {
            
            temperr=pthread_mutex_unlock(&(cache->mem_check_mtx));      // unlocking cache's mem-mtx
            if(temperr) {LOG_ERR(temperr, "lookup-write: unlocking cache's mem-mutex"); return ERR;}

            // invoking the replacement algorithm
            if((temperr=sc_algorithm(cache, (*bytes_used), replaced_files, FALSE, file_name))!=SUCCESS) {
                if(errno!=ENOSPC) return ERR;   // fatal error
                res=ENOSPC;
            }
        }
        else {  // else (free space is enough), pre-allocate the element
            cache->curr_byte_size+=(*bytes_used);       // incrementing cache's size
            if((cache->curr_byte_size)>max_byte_size_reached) max_byte_size_reached=cache->curr_byte_size;

            temperr=pthread_mutex_unlock(&(cache->mem_check_mtx));      // unlocking cache's mem-mtx
            if(temperr) {LOG_ERR(temperr, "lookup-write: unlocking cache's mem-mutex"); return ERR;}
        }
    }


    // ########## MAIN OPERATION ##########
    // getting the starting index for the lookup operation
    unsigned int idx=conc_hash_hashfun((file_name), (ht->size));
    while(res==PROBING) {
        // locking the hashtable entry
        temperr=pthread_mutex_lock(&(((ht->table)[idx]).entry_mtx));
        if(temperr) {LOG_ERR(temperr, "lookup: locking hashtable's entry"); return ERR;}

        // if the entry is NULL, the element is not in the hashtable
        if(((ht->table)[idx]).entry==NULL) res=ENOENT;

        // if the entry has been deleted, continue probing
        else if (((ht->table)[idx]).entry==ht->mark) {}

        // if there's a match, perform the correct operation base on the op_code
        else if(!strcmp(file_name, ((file*)ht->table[idx].entry)->name)) {
            temp_file=(file*)ht->table[idx].entry;  // shortcut to current file
            ht->table[idx].r_used=nth_chance-1;    // since the file has just been referred, update its used-bit

            // Calls a different function based on the operation flag
            if(op==OPEN_F) {    // opens the file if it's not locked
                res=open_file(temp_file, usr_id);
                if(res==ERR) {LOG_ERR(errno, "lookup: opening file"); return ERR;}   // a fatal error (memerr/mutexerr) has occurred
            }

            else if(op==OPEN_C_F) res=EEXIST;    // checking this means the file already exists, so return error

            else if(op==READ_F) {   // if file open returns its data, else returns error
                res=read_file(temp_file, data_read, bytes_used, usr_id);
                if(res==ERR) {LOG_ERR(errno, "lookup: reading file"); return ERR;}  // a fatal error (memerr/mutexerr) has occurred
            }

            else if(op==WRITE_F) {  // if last operation on the file was create&lock, writes the file's content
                res=write_file(temp_file, data_written, bytes_used, usr_id);
                if(res==ERR) {LOG_ERR(errno, "lookup: writing file"); return ERR;}  // a fatal error (memerr/mutexerr) has occurred
            }

            else if(op==WRITE_F_APP) {  // if the file isn't locked by another user, writes data in append
                res=write_append(temp_file, data_written, bytes_used, usr_id);
                if(res==ERR) {LOG_ERR(errno, "lookup: writing file in append"); return ERR;}    // a fatal error (memerr/mutexerr) has occurred
            }

            else if(op==LOCK_F) {   // attempts to lock the file
                res=lock_file(temp_file, usr_id);   // if file is unlocked, locks it
                if(res==ERR) {LOG_ERR(errno, "lookup: locking file"); return ERR;}  // a fatal error (memerr/mutexerr) has occurred
            }

            else if(op==UNLOCK_F) res=unlock_file(temp_file, usr_id);    // if file is locked by the user, unlocks it
            
            else if(op==RM_F) {     // if the file is locked by the user, deletes it from the cache
                size_t file_size=temp_file->file_size;  // saving file size for later
                res=remove_file(temp_file, usr_id);        // if file is locked by the user, deletes it
                if(res==ERR) {LOG_ERR(errno, "lookup: removing file"); return ERR;} // a fatal error (memerr/mutexerr) has occurred
                if(res==SUCCESS) {
                    ht->table[idx].entry=ht->mark;  // the item has been deleted, so mark it as deleted

                    temperr=pthread_mutex_lock(&(cache->mem_check_mtx));
                    if(temperr) {LOG_ERR(temperr, "lookup-r: locking cache's mem-mutex"); return ERR;}

                    cache->curr_file_number--;  // decreasing the file count by 1
                    cache->curr_byte_size-=file_size;   // decreasing the total file size by the size of the deleted file

                    temperr=pthread_mutex_unlock(&(cache->mem_check_mtx));
                    if(temperr) {LOG_ERR(temperr, "lookup-r: unlocking cache's mem-mutex"); return ERR;}
                }
            }

            else if(op==CLOSE_F) {  // closes the file for the user
                res=close_file(temp_file, usr_id);     // closes file for the user
                if(res==ERR) {LOG_ERR(errno, "lookup: closing file"); return ERR;}  // a fatal error (memerr/mutexerr) has occurred
            }

            // disables the writeFile operation (at this point, the first operation has been executed)
            // note: if the file was deleted, it now unaccessible
            if(res==SUCCESS && op!=RM_F) temp_file->f_write=0;
        }
        
        // else, continue probing
        temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));
        if(temperr) {LOG_ERR(temperr, "lookup: unlocking hashtable's entry"); return ERR;}

        idx++;
        idx=idx%(ht->size*2);
    }

    // if the operation was an "open" or a "close", register the file opening/closing
    if(op==OPEN_F || op==OPEN_L_F) {
        if((temperr=conc_fifo_push(open_files[*usr_id], (void*)strdup(file_name)))==ERR) {
            LOG_ERR(errno, "register open: saving open name");
            return ERR;
        }
    }



    if(res!=SUCCESS) goto cleanup_lookup;
    return res;     // returns the result of the operation performed

// CLEANUP SECTION
cleanup_lookup:
    if(op==WRITE_F || op== WRITE_F_APP) {
        if(res!=EFBIG && res!=ENOSPC) {
            // de-allocating the pre-allocated memory
            temperr=pthread_mutex_lock(&(cache->mem_check_mtx));
            if(temperr) {LOG_ERR(temperr, "lookup-write: locking cache's mem-mutex"); return ERR;}
            cache->curr_byte_size-=(*bytes_used);
            temperr=pthread_mutex_unlock(&(cache->mem_check_mtx));
            if(temperr) {LOG_ERR(temperr, "lookup-write: unlocking cache's mem-mutex"); return ERR;}
        }
    }
    errno=res;
    return res;
}


// returns the first N files (or all files if less than N) from the cache
int sc_return_n_files(sc_cache* cache, const int N, file*** returned_files) {
    int temperr;
    unsigned i, j;     // for loop indexes
    unsigned file_count=0;  // will hold count of the returned files
    conc_hash_table* ht=cache->ht;  // shortcut to the hashtable
    file* temp_file=NULL;   // file to copy

    // allocating the array of returned files
    (*returned_files)=(file**)calloc(N, sizeof(file*));
    if(!(*returned_files)) {LOG_ERR(errno, "return_n_files: allocating space for returned files"); return ERR;}
    for(i=0; i<N; i++) { (*returned_files)[i]=NULL;}   // setting all pointers to NULL

    for(i=0; (i<(cache->max_file_number*2))&&(file_count<N); i++) {
        // locking the mutex of the current entry
        temperr=pthread_mutex_lock(&(((ht->table)[i]).entry_mtx));
        if(temperr) {LOG_ERR(temperr, "return_n_files: locking hashtable's entry"); return ERR;}

        if((ht->table)[i].entry && (ht->table)[i].entry!=ht->mark) {
            temp_file=(file*)(ht->table)[i].entry;  // shorcut to the i-th file

            // allocating a file to hold the copy of the temp
            (*returned_files)[file_count]=file_create(temp_file->name);
            if(!(*returned_files)[file_count]) {LOG_ERR(errno, "return_n_files: allocating file"); return ERR;}
            
            if(temp_file->file_size) {  // if the file wasn't null, copies its data onto the new one
                ((*returned_files)[file_count])->file_size=temp_file->file_size;    // setting file dimension
                // allocating space for the file data
                ((*returned_files)[file_count])->data=(byte*)calloc(temp_file->file_size, sizeof(byte));
                if(!((file*)((*returned_files)[file_count])->data)) {LOG_ERR(errno, "return_n_files: allocating file data"); return ERR;}
                // copying data from the original file to the copy
                for(j=0; j<(temp_file->file_size); j++) ((*returned_files)[file_count])->data[j]=(temp_file->data)[j];
            }
            file_count++;   // incrementing the file count
        }

        // unlocking current entry
        temperr=pthread_mutex_unlock(&(((ht->table)[i]).entry_mtx));
        if(temperr) {LOG_ERR(temperr, "return_n_files: unlocking hashtable's entry"); return ERR;}
    }

    return SUCCESS;
}


// creates a new file with @pathname as its name and returns it
file* file_create(const char* pathname) {
    file* new_file=(file*)malloc(sizeof(file));
    new_file->data=NULL;
    new_file->f_lock=0;
    new_file->f_open=0;
    new_file->f_write=0;
    new_file->file_size=0;
    new_file->name=(char*)calloc(strlen(pathname)+1, sizeof(char));
    if(!new_file) return NULL;  // mem error
    strcpy((new_file->name), pathname);
    new_file->open_list=ll_create();
    if(!(new_file->open_list)) return NULL; // mem error

    return new_file;
}


// closes every file opened by the user having the ID passed as param
int usr_close_all(sc_cache* cache, const int* usr_id) {
    char* temp_file=NULL;
    
    while((temp_file=(char*)conc_fifo_pop(open_files[*usr_id]))) {
        if((sc_lookup(cache, temp_file, CLOSE_F, usr_id, NULL, NULL, NULL, NULL)==ERR)
            && errno!=ENOENT) {
            LOG_ERR(errno, "closing all opened files");
            return ERR;
        }
        free(temp_file);
    }


    return SUCCESS;
}


// completely wipes the cache, deallocating everything in it WARNING: non concurrent
int sc_cache_clean(sc_cache* cache) {
    if(!cache || !(cache->ht)) {
        LOG_ERR(EINVAL, "uninitialized cache/ht");
        return ERR;
    }

    conc_hash_table* ht=cache->ht;  // shortcut
    int temperr;
    int i;

    if(ht->table) {
        for(i=0; i<(ht->size*2); i++) {
            if(((ht->table)[i].entry) && ((ht->table)[i].entry)!=(ht->mark)) {
                file* temp=(file*)ht->table[i].entry;
                if(temp->data) free(temp->data);
                if(temp->name) free(temp->name);
                if(temp->open_list) {
                    temperr=ll_dealloc_full(temp->open_list);
                    if(temperr==ERR) return ERR;    // fatal error
                }
                free(temp);
            }
        }
        free(ht->table);
    }
    if(ht->mark) free(ht->mark);
    free(ht);

    if((temperr=pthread_mutex_destroy(&(cache->mem_check_mtx)))==ERR) return ERR;
    free(cache);


    return SUCCESS;
}



// ########## CACHE OPERATIONS ##########


// helper function: opens the file for the user; if it was already opened, returns successfully
static int open_file(file* file, const int* usr_id) {
    int temperr;

    int* real_id=(int*)malloc(sizeof(int));
    if(!real_id) return ERR;    // fatal error, errno alreadyset by the call
    *real_id=*usr_id;

    if((temperr=ll_insert_head(&(file->open_list), (void*)real_id, int_ptr_cmp))==ERR)  // insert usr_id in the file_open list
    return ERR;   // fatal error, errno already set by the call


    return SUCCESS;
}


// helper function: returns a copy of the file's data if previously opened
static int read_file(file* file_to_read, byte** data_read, size_t* bytes_used, const int* usr_id) {
    int i;  // index for loop
    int res;

    res=ll_search(file_to_read->open_list, (void*)usr_id, int_ptr_cmp);     // checking if the file has been opened by the user
    if(!res) return EPERM;      // file not opened: operation not permitted
    if(res==ERR) return ERR;    // fatal error, errno already set by the call

    // file was opened by the user, so they can read it
    if(file_to_read->file_size) {   // if the file has any data, copies it into the buffer passed, allocating it first
        (*data_read)=(byte*)malloc((file_to_read->file_size)*sizeof(byte));
        if(!(*data_read)) return ERR;  // fatal error, errno already set by the call

        // copying data from file to buffer
        for(i=0; i<(file_to_read->file_size); i++) (*data_read)[i]=(file_to_read->data)[i];
    }
    else (*data_read)=NULL;     // file was empty
    (*bytes_used)=file_to_read->file_size;  // memorizing the size of the file read to send it to the user

    return SUCCESS;
}


// helper function: if file is unlocked, locks the file; if user has the lock, returns successfully
static int lock_file(file* file, const int* usr_id) {
    int res;

    if(file->f_lock && (file->f_lock)!=(*usr_id)) return EBUSY;    // lock is held by another user
    if(file->f_lock && (file->f_lock)==(*usr_id)) return SUCCESS;  // user already has the lock

    res=ll_search(file->open_list, (void*)usr_id, int_ptr_cmp);     // checking if the file has been opened by the user
    if(!res) return EPERM;      // file not opened: operation not permitted
    if(res==ERR) return ERR;    // fatal error, errno already set by the call

    file->f_lock=(*usr_id);    // locks the file for the user


    return SUCCESS;
}


// helper function: if the lock is held by the user, releases it; else, return an error
static int unlock_file(file* file, const int* usr_id) {
    int res;

    if(file->f_lock!=(*usr_id)) return EPERM;   // user wasn't holding the lock

    res=ll_search(file->open_list, (void*)usr_id, int_ptr_cmp);     // checking if the file has been opened by the user
    if(!res) return EPERM;      // file not opened: operation not permitted
    if(res==ERR) return ERR;    // fatal error, errno already set by the call

    file->f_lock=0;     // resets lock
    return SUCCESS;
}


// helper function: if the user has a lock on the file, deletes it from the server
static int remove_file(file* file, const int* usr_id) {
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
static int close_file(file* file, const int* usr_id) {
    int temperr;
    int res;

    res=ll_search(file->open_list, (void*)usr_id, int_ptr_cmp);     // checking if the file has been opened by the user
    if(!res) return EPERM;      // file not opened: operation not permitted
    if(res==ERR) return ERR;    // fatal error, errno already set by the call

    if(file->f_lock==(*usr_id)) file->f_lock=0;    // if the user has a lock on the file, unlock it before closing it
    if((temperr=ll_remove(&(file->open_list), (void*)usr_id, int_ptr_cmp))==ERR)  // remove usr_id from the file_open list
    return ERR;   // fatal error, errno already set by the call

    return SUCCESS;
}


// helper function: writes the whole file if the last operation on it was create&lock
static int write_file(file* file, const byte* data_written, const size_t* bytes_used, const int* usr_id) {
    if(!file->f_write || file->f_lock!=(*usr_id)) return EPERM;    // last operation was not create&lock

    int i;  // for loop index

    file->data=(byte*)malloc((*bytes_used)*sizeof(byte));
    if(!file->data) {LOG_ERR(errno, "writing file: memerr"); return ERR;}
    file->file_size=(*bytes_used);  // updating file size

    for(i=0; i<(*bytes_used); i++) {    // writing data into the file
        (file->data)[i] = data_written[i];
    }

    return SUCCESS;
}


// helper function: writes the data passed as arg in append mode
static int write_append(file* file, const byte* data_written, const size_t* bytes_used, const int* usr_id) {
    if(file->f_lock && (file->f_lock)!=(*usr_id)) return EPERM;    // another user has a lock on this file

    int i=0, j;  // for loop index
    int res;

    res=ll_search(file->open_list, (void*)usr_id, int_ptr_cmp);     // checking if the file has been opened by the user
    if(!res) return EPERM;      // file not opened: operation not permitted
    if(res==ERR) return ERR;    // fatal error, errno already set by the call

    size_t write_size=file->file_size+(*bytes_used);
    byte* updated_data=(byte*)malloc(write_size*sizeof(byte));
    if(!updated_data) return ERR;   // fatal error, errno already set by the call

    // if the file already has some data in it, copy it to the updated version
    if(file->file_size) for(i=0; i<(file->file_size); i++) updated_data[i]=(file->data)[i];
    for(j=0; i<write_size && j<(*bytes_used); i++, j++) updated_data[i]=data_written[j];

    if(file->data) free(file->data);    // destroying old data
    file->data=updated_data;    // updating file's data
    file->file_size=write_size;     // updating file's size

    return SUCCESS;
}



// ########## AUXILIARY FUNCTIONS ##########


// helper function: returns TRUE if the files' names are equal, FALSE otherwise
static int is_duplicate(const file* file1, const file* file2) {
    // NULL checks already done in the insert procedure
    if(!strcmp(file1->name, file2->name)) return TRUE;
    else return FALSE;
}


// helper function: compares two void pointers as integers
static int int_ptr_cmp(const void* x, const void* y) {
    // warning: segfault if one of the pointers is null
    if((*(int*)x)==(*(int*)y)) return 0;    // x=y
    else if((*(int*)x)>(*(int*)y)) return 1;    // x>y
    else return -1;      // x<y
}
