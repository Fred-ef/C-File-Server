#include "sc_cache.h"

// TODO -   gestire duplicati
// TODO -   de-allocare il file quando l'inserimento fallisce
// TODO -   de-allocare lo spazio quando la modifica fallisce


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
    search_result res=NOT_FOUND;    // termination condition
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
    while(res==NOT_FOUND) {       // finding a free entry for the file
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
        else if((temperr=sc_cache_is_duplicate(((ht->table)[idx]).entry, new_file))) {  // TODO implementare
            LOG_DEBUG("THREAD: %d\tKEY %d WAS DUPLICATE!!!\n", (gettid()), idx);
            res=FAILED; errno=EEXIST;
        }

        // else, continue probing
        temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));      // unlocking the hashtable entry
        if(temperr) {LOG_ERR(temperr, "insert: unlocking hashtable's entry"); return ERR;}

        idx++;
        idx=idx%(ht->size*2);
    }   // HASH SECTION END


    // if the insertion failed, return an error
    if(res==FAILED) goto cleanup_insert;     // TODO deallocare la memoria
    LOG_DEBUG("INSERTED IN LIST\n");       // TODO REMOVE
    return SUCCESS;

// CLEANUP SECTION
cleanup_insert:
// de-allocating the pre-allocated memory
    temperr=pthread_mutex_lock(&(cache->mem_check_mtx));
    if(temperr) {LOG_ERR(temperr, "insert: locking cache's mem-mutex"); goto cleanup_insert;}
    cache->curr_file_number--;
    cache->curr_byte_size-=new_file->file_size;
    temperr=pthread_mutex_unlock(&(cache->mem_check_mtx));
    if(temperr) {LOG_ERR(temperr, "insert: unlocking cache's mem-mutex"); goto cleanup_insert;}
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


int sc_lookup(sc_cache* cache, char* file_name, op_code op, file** file_read, file* file_to_write) {
    if(!cache) {LOG_ERR(EINVAL, "lookup: cache is uninitialized");}
    if(!file_name) {LOG_ERR(EINVAL, "lookup: invalid file name specified");}
    if(op==READ_F && !file_read) {LOG_ERR(EINVAL, "lookup: invalid pointer provided");}
    if((op==WRITE_F || op==WRITE_F_APP) && !file_to_write) {LOG_ERR(EINVAL, "lookup: invalid pointer provided");}

    int temperr;    // used for error codes
    search_result res=0;    // termination condition (in case you don't like break;)
    conc_hash_table* ht=cache->ht;      // shortcut to the hashtable


    // getting the starting index for the lookup operation
    unsigned int idx=conc_hash_hashfun((file_name), (ht->size));
    while(res==NOT_FOUND) {
        // locking the hashtable entry
        temperr=pthread_mutex_lock(&(((ht->table)[idx]).entry_mtx));
        if(temperr) {LOG_ERR(temperr, "lookup: locking hashtable's entry"); return ERR;}

        // if the entry is NULL, the element is not in the hashtable
        if(((ht->table)[idx]).entry==NULL) {errno=ENOENT; res=FAILED;}

        // if there's a match, perform the correct operation base on the op_code
        else if(!strcmp(file_name, ((file*)ht->table[idx].entry)->file_size)) {
            // CHIAMATA ALLA FUNZIONE DESIDERATA
            // RICORDARE DI AGGIORNARE LO USED BIT
            if(op==OPEN_F) {}
            else if(op==OPEN_C_F) {}
            else if(op==READ_F) {}
            else if(op==WRITE_F) {}
            else if(op==LOCK_F) {}
            else if(op==UNLOCK_F) {}
            else if(op==RM_F) {}
            else if(op==CLOSE_F) {}
        }
        
        // else, continue probing
        temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));
        if(temperr) {LOG_ERR(temperr, "lookup: unlocking hashtable's entry"); return ERR;}

        idx++;
        idx=idx%(ht->size*2);
    }


    // if the operation hasn't been completed successfully, return an error
    if(res==FAILED) return ERR;

    return SUCCESS;

// CLEANUP SECTION
cleanup_lookup:
    return ERR;
}