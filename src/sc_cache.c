#include "sc_cache.h"

// TODO -   gestire duplicati


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


int sc_cache_insert(sc_cache* cache, file* new_file, file** replaced_files) {
    if(!cache || !new_file) {LOG_ERR(EINVAL, "insert: cache_push args cannot be NULL"); goto cleanup_insert;}
    if(!(cache->ht)) {LOG_ERR(EINVAL, "insert: cache's hashtable not initialized");}

    int temperr;    // used for error codes
    conc_hash_table* ht=cache->ht;      // shortcut to the hashtable
    
    // getting the starting index for our file
    unsigned int idx=conc_hash_hashfun((new_file->name), (ht->size));
    while(TRUE) {       // finding a free entry for the file
        // locking the hashtable entry
        temperr=pthread_mutex_lock(&(((ht->table)[idx]).entry_mtx));
        if(temperr) {LOG_ERR(temperr, "insert: locking hashtable's entry"); goto cleanup_insert;}

        // cheking if the entry is free, locking the hashtable entry
        if(((ht->table)[idx]).entry==NULL || ((ht->table)[idx]).entry==(ht->mark)) {

            // checking if there's enough space in the cache to memorize the file
            temperr=pthread_mutex_lock(&(cache->mem_check_mtx));
            if(temperr) {LOG_ERR(temperr, "insert: locking cache's mem-mutex"); goto cleanup_insert;}

            // if the table is full, invoke the replacement algorithm
            if(((cache->curr_file_number + 1) > cache->max_file_number)
            || ((cache->curr_byte_size + new_file->file_size) > cache->max_byte_size)) {

                temperr=pthread_mutex_unlock(&(cache->mem_check_mtx));      // unlocking cache's mem-mtx
                if(temperr) {LOG_ERR(temperr, "insert: unlocking cache's mem-mutex"); goto cleanup_insert;}
                // CHIAMA ALGORITMO DI RIMPIAZZAMENTO
            }
            // if the table is not full, insert the element
            else {
                ((ht->table)[idx]).entry=(void*)new_file;
                ((ht->table)[idx]).r_used=1;
                cache->curr_file_number++;
                cache->curr_byte_size+=(new_file->file_size);

                LOG_DEBUG("PUSHED CORRECTLY\n\n#FILE: %d\t#BYTES: %d\n", (cache->curr_file_number), (cache->curr_byte_size));     // TODO REMOVE
                // unlocking cache's mem-mtx
                temperr=pthread_mutex_unlock(&(cache->mem_check_mtx));
                if(temperr) {LOG_ERR(temperr, "insert: unlocking cache's mem-mutex"); goto cleanup_insert;}
                break;      // exiting the while-loop
            }

        }

        // if the element is not free and it is a duplicate, return an error
        else if((temperr=sc_cache_is_duplicate(((ht->table)[idx]).entry, new_file))) {  // TODO implementare
            LOG_DEBUG("THREAD: %d\tKEY %d WAS DUPLICATE!!!\n", (gettid()), idx);
            temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));      // unlocking the hashtable entry
            if(temperr==TRUE) {LOG_ERR(EEXIST, "insert: the element is a duplicate");}
            else {LOG_ERR(EINVAL, "insert: error while comparing elements");}
            goto cleanup_insert;
        }

        // continue probing
        else {
            temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));      // unlocking the hashtable entry
            if(temperr) {LOG_ERR(temperr, "insert: unlocking hashtable's entry"); goto cleanup_insert;}
            idx++;
            idx=idx%(ht->size*2);
        }
    }   // HASH SECTION END
    
    
    LOG_DEBUG("INSERTED IN LIST\n");       // TODO REMOVE
    return SUCCESS;

// CLEANUP SECTION
cleanup_insert:
    return ERR;
}

int sc_algorithm(sc_cache* cache, unsigned size_var, file** replaced_files) {
    if(!cache) {LOG_ERR(EINVAL, "replace: cache not initialized"); goto cleanup_alg;}

    int i, j;          // loop indexes
    int temperr;    // used for code errors
    conc_hash_table* ht=cache->ht;

    // cycling through every entry, for 2 times at max
    for(i=0; i<nth_chance; i++) {       // perform twice if the first visit was unsuccessful

        for(j=0; j<((cache->ht->size)*2); j++) {    // scan the whole hashtable, searching for files to replace
            // locking current entry
            temperr=pthread_mutex_lock(&(((ht->table)[j]).entry_mtx));
            if(temperr) {LOG_ERR(temperr, "replace: locking hashtable's entry"); goto cleanup_alg;}

            if((ht->table)[j].entry && (ht->table)[j].entry!=ht->mark) {
                if((ht->table)[j].r_used) (ht->table)[j].r_used--;  // if the item was recently used, reset its used-bit
                else {
                    // locking cache's mem-mtx
                    temperr=pthread_mutex_lock(&(cache->mem_check_mtx));
                    if(temperr) {LOG_ERR(temperr, "replace: locking cache's mem-mutex"); goto cleanup_alg;}

                    // deleting the file and de-allocating its space
                }
            } 
        }
    }

    return SUCCESS;

// CLEANUP SECTION
cleanup_alg:
    return ERR;
}