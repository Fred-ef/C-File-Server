#include "lru_cache.h"
char** strings=NULL;


// returns an LRU-cache of the given size
lru_cache* lru_cache_create(int max_file_number, int max_byte_size) {
    lru_cache* cache=(lru_cache*)malloc(sizeof(lru_cache));
    if(!cache) goto cache_create_cleanup;

    cache->queue=conc_fifo_create(NULL);            // allocates the cache's queue
    if(!(cache->queue)) goto cache_create_cleanup;

    cache->max_file_number=max_file_number;     // set the maximum numbers of file allowed
    cache->max_byte_size=max_byte_size;         // set the maximum memory usable
    cache->curr_file_number=0;      // the cache is initially empty
    cache->curr_byte_size=0;        // the cache is initially empty

    int temperr=pthread_mutex_init(&(cache->mem_check_mtx), NULL);      // initializes cache's memory-related mutex
    if(temperr) {errno=temperr; goto cache_create_cleanup;}

    return cache;       // returns the cache built

cache_create_cleanup:
    cleanup_pointers(cache, (cache->queue), NULL);
    return NULL;
}


// pushes the given file inside the cache
int lru_cache_push(lru_cache* cache, file* file) {
    if(!cache || !file) {errno=EINVAL; return ERR;}
    if(!(cache->queue)) {errno=EINVAL; return ERR;}

    int temperr, found=0;       // auxiliary variables
    conc_queue* queue=cache->queue;     // shortcut to cache's queue
    conc_node newelement=conc_node_create((void*)file);     // creating an entry out of the file passed as parameter
    if(!newelement) return ERR;

    conc_node aux1=queue->head, aux2;       // shortcut to queue's head
    temperr=pthread_mutex_lock(&(aux1->node_mtx));
    if(temperr) {errno=temperr; free(newelement); return ERR;}

    // then push the element into the queue as well
    if(aux1->next) {        // if the list is NOT EMPTY, we need to navigate to its end
        while(aux1->next!=NULL) {       // scan untill the end of the queue
            aux2=aux1;
            aux1=aux1->next;
            temperr=pthread_mutex_lock(&(aux1->node_mtx));
            if(temperr) {errno=temperr; free(newelement); return ERR;}
            temperr=pthread_mutex_unlock(&(aux2->node_mtx));
            if(temperr) {errno=temperr; free(newelement); return ERR;}
        }
    }


    // check if there is enough space to memorize the current file
    temperr=pthread_mutex_lock(&(cache->mem_check_mtx));
    if (temperr) {errno=temperr; goto cache_push_cleanup_3;}

    // if the table is full, return an error
    if(((cache->curr_file_number + 1) > cache->max_file_number) || ((cache->curr_byte_size + file->file_size) > cache->max_byte_size)) {
        errno=ENOMEM;
        goto cache_push_cleanup_1;
    }
    // if the table is NOT FULL, insert the element
    else {
        ((ht->table)[idx]).entry=newelement;
        cache->curr_file_number++;
        cache->curr_byte_size+=(file->file_size);
        printf("PUSHED CORRECTLY\n\n#FILE: %d\t#BYTES: %d\n", (cache->curr_file_number), (cache->curr_byte_size));     // TODO REMOVE
        temperr=pthread_mutex_unlock(&(cache->mem_check_mtx));
        if (temperr) {errno=temperr; goto cache_push_cleanup_2;}
        found=1;
    }


    // if the element is already cached, return an error
    else if((temperr=lru_cache_is_duplicate(((ht->table)[idx]).entry, newelement))) {
        printf("THREAD: %d\tKEY %d WAS DUPLICATE!!!\n", (gettid()), idx);
        if(temperr==TRUE) errno=EEXIST;
        else errno=EINVAL;
        goto cache_push_cleanup_2;
    }






    

    // when at the tail, insert the element
    aux1->next=newelement;
    temperr=pthread_mutex_unlock(&(aux1->node_mtx));
    if(temperr) {errno=temperr; goto cache_push_cleanup_4;}
    printf("INSERTED IN LIST\n");       // TODO REMOVE

    return SUCCESS;

    // ERROR CLEANUP SECTION
cache_push_cleanup_1:
    temperr=pthread_mutex_unlock(&(cache->mem_check_mtx));
    if (temperr) {errno=temperr; free(newelement); return ERR;}
cache_push_cleanup_2:
    temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));
    if (temperr) {errno=temperr; free(newelement); return ERR;}
cache_push_cleanup_3:
    temperr=pthread_mutex_unlock(&(aux1->node_mtx));
    if(temperr) {errno=temperr; free(newelement); return ERR;}
cache_push_cleanup_4:
    free(newelement);
    return ERR;
}


// pops the last-recently-used elements from the cache and returns it
file* lru_cache_pop(lru_cache* cache) {
    if(!cache) {errno=EINVAL; return NULL;}
    if(!(cache->ht) || !(cache->queue)) {errno=EINVAL; return NULL;}

    conc_queue* queue=cache->queue;
    conc_hash_table* ht=cache->ht;
    file* file=NULL;

    int temperr;
    unsigned int idx;
    temperr=pthread_mutex_lock(&((queue->head)->node_mtx));
    if(temperr) {errno=temperr; goto cache_pop_cleanup_4;}
    // creating a shortcut for the element to pop
    conc_node aux=(queue->head)->next;

    // if the cache is empty, fail returning error
    if(!(aux)) {
        errno=ENOENT;
        goto cache_pop_cleanup_3;
    }

    // if the cache is NOT EMPTY, extract its head and return it
    else {
        temperr=pthread_mutex_lock(&(aux->node_mtx));
        if(temperr) {errno=temperr; goto cache_pop_cleanup_3;}

        file=aux->data;
        idx=conc_hash_hashfun((file->name), (ht->size));

        while(1) {
            temperr=pthread_mutex_lock(&(((ht->table)[idx]).entry_mtx));
            if (temperr) {errno=temperr; goto cache_pop_cleanup_2;}

            // if we encounter a NULL value, then the element is not cached
            if(((ht->table)[idx]).entry==NULL) {errno=ENOENT; goto cache_pop_cleanup_2;}
            // if we encounter a deleted element, we can go on to the next cycle
            else if(((ht->table)[idx]).entry==ht->mark);

            // if we find the element, we mark it as deleted and update cache's size
            else if((temperr=lru_cache_is_duplicate(((ht->table)[idx]).entry, aux))) {
                if(temperr==TRUE) {
                    // update cache's size
                    temperr=pthread_mutex_lock(&(cache->mem_check_mtx));
                    if (temperr) {errno=temperr; goto cache_pop_cleanup_1;}
                    // mark the element as deleted
                    ((ht->table)[idx]).entry=ht->mark;
                    cache->curr_file_number--;
                    cache->curr_byte_size-=(file->file_size);
                    // updating head pointer
                    (queue->head)->next=aux->next;
                    printf("POPPED CORRECTLY\n\n#FILE: %d\t#BYTES: %d\n", (cache->curr_file_number), (cache->curr_byte_size));     // TODO REMOVE
                    temperr=pthread_mutex_unlock(&(cache->mem_check_mtx));
                    if (temperr) {errno=temperr; goto cache_pop_cleanup_1;}
                    // the entry has been eliminated, we can now exit the cycle
                    break;
                }
                else {errno=EINVAL; goto cache_pop_cleanup_1;}
            }

            // if we encountered the wrong element, continue probing
            temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));
            if (temperr) {errno=temperr; goto cache_pop_cleanup_2;}
            idx++;
            idx=idx%(ht->size*2);
        }
    }

    temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));
    if (temperr) {errno=temperr; goto cache_pop_cleanup_2;}
    temperr=pthread_mutex_unlock(&((queue->head)->node_mtx));
    if(temperr) {errno=temperr; goto cache_pop_cleanup_2;}
    temperr=pthread_mutex_unlock(&(aux->node_mtx));
    if(temperr) {errno=temperr; goto cache_pop_cleanup_4;}

    return (file=conc_node_destroy(aux));

    // ERROR CLEANUP SECTION
cache_pop_cleanup_1:
    temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));
    if(temperr) errno=temperr;
cache_pop_cleanup_2:
    temperr=pthread_mutex_unlock(&(aux->node_mtx));
    if(temperr) errno=temperr;
cache_pop_cleanup_3:
    temperr=pthread_mutex_unlock(&((queue->head)->node_mtx));
    if(temperr) errno=temperr;
cache_pop_cleanup_4:
    return NULL;
}


// if present, returns the file with the given name
file* lru_cache_lookup(lru_cache* cache, const char* filename) {
    if(!cache || !filename) {errno=EINVAL; return NULL;}
    if(!(cache->ht) || !(cache->queue)) {errno=EINVAL; return NULL;}

    conc_hash_table* ht=cache->ht;
    file* file=NULL;
    
    int temperr;
    unsigned int idx=conc_hash_hashfun(filename, (ht->size));
    while(1) {
        temperr=pthread_mutex_lock(&(((ht->table)[idx]).entry_mtx));
        if (temperr) {errno=temperr; return NULL;}

        // if we encounter a NULL value, then the element is not cached
        if(((ht->table)[idx]).entry==NULL) {errno=ENOENT; break;}
        // if we encounter a deleted element, we can go on to the next cycle
        else if(((ht->table)[idx]).entry==ht->mark);
        // if we find the element, we can return it
        else if(lru_cache_is_file_name(((ht->table)[idx].entry), filename)) {
            printf("TROVATOOOOOOOOOO\n\n");
            file=((ht->table)[idx]).entry->data;
            break;
        }
        // if we encountered the wrong element, continue probing
        temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));
        if (temperr) {errno=temperr; return NULL;}
        idx++;
        idx=idx%(ht->size*2);
    }

    temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));
    if (temperr) {errno=temperr; return NULL;}

    return file;        // returns either the file or NULL if the lookup failed
}


// if present, removes the file with the given name from the cache and returns it
file* lru_cache_remove(lru_cache* cache, const char* filename) {
    if(!cache || !filename) {errno=EINVAL; return NULL;}
    if(!(cache->ht) || !(cache->queue)) {errno=EINVAL; return NULL;}

    int temperr;
    file* file=NULL;
    conc_queue* queue=cache->queue;     // shortcut to cache's queue
    conc_hash_table* ht=cache->ht;      // shortcut to cache's hashtable

    conc_node aux_node=NULL, aux1=NULL, aux2=NULL;
    unsigned int idx=lru_cache_addr_lookup(cache, filename, &aux_node);
    if(!aux_node || idx==(unsigned)ERR) return NULL;

    aux2=queue->head;
    temperr=pthread_mutex_lock(&(aux2->node_mtx));
    if(temperr) {errno=temperr; return NULL;}

    // if the list is empty, return a not-found error
    if(!(aux2->next)) {errno=ENOENT; goto cache_remove_cleanup_2;}
    aux1=(queue->head)->next;

    while((aux1!=NULL) && (aux1!=aux_node)) {       // scan untill the element (or NULL value) is found
        // first lock the next element
        temperr=pthread_mutex_lock(&(aux1->node_mtx));
        if(temperr) {errno=temperr; goto cache_remove_cleanup_2;}
        // then unlock the previous
        temperr=pthread_mutex_unlock(&(aux2->node_mtx));
        if(temperr) {errno=temperr; goto cache_remove_cleanup_1;}
        // then update both pointers
        aux2=aux1;
        aux1=aux1->next;
    }

    // if the queue has been scanned (or the element has been removed meanwhile) return an error
    if(!aux1) {errno=ENOENT; goto cache_remove_cleanup_2;}

    // if the element has been found, proceed to lock it and remove it
    temperr=pthread_mutex_lock(&(aux1->node_mtx));
    if(temperr) {errno=temperr; goto cache_remove_cleanup_2;}

    // locking the hash table entry relative to the element
    temperr=pthread_mutex_lock(&(((ht->table)[idx]).entry_mtx));
    if(temperr) {errno=temperr; goto cache_remove_cleanup_4;}

    // if the element has meanwhile been removed/substituted, return an error
    if(!(((ht->table)[idx]).entry) || (((ht->table)[idx]).entry)==(ht->mark) || (((ht->table)[idx]).entry)!=aux_node)
            {errno=ENOENT; goto cache_remove_cleanup_3;}

    // updating cache memory information
    temperr=pthread_mutex_lock(&(cache->mem_check_mtx));
    if (temperr) {errno=temperr; goto cache_remove_cleanup_3;}

    // actual removal of the element
    file=((ht->table)[idx]).entry->data;
    ((ht->table)[idx]).entry=ht->mark;
    cache->curr_file_number--;
    cache->curr_byte_size-=file->file_size;
    printf("REMOVED CORRECTLY\n\n#FILE: %d\t#BYTES: %d\n", (cache->curr_file_number), (cache->curr_byte_size));     // TODO REMOVE
    // removal from the queue
    aux2->next=aux1->next;

    temperr=pthread_mutex_unlock(&(cache->mem_check_mtx));
    if (temperr) {errno=temperr; goto cache_remove_cleanup_3;}
    temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));
    if(temperr) {errno=temperr; goto cache_remove_cleanup_4;}
    temperr=pthread_mutex_unlock(&(aux2->node_mtx));
    if(temperr) {errno=temperr; goto cache_remove_cleanup_1;}
    temperr=pthread_mutex_unlock(&(aux1->node_mtx));
    if(temperr) {errno=temperr; return NULL;}


    return (file=conc_node_destroy(aux1));

    // ERROR CLEANUP SECTION
cache_remove_cleanup_1:
    temperr=pthread_mutex_unlock(&(aux1->node_mtx));
    if(temperr) errno=temperr;
    return NULL;
cache_remove_cleanup_2:
    temperr=pthread_mutex_unlock(&(aux2->node_mtx));
    if(temperr) errno=temperr;
    return NULL;
cache_remove_cleanup_3:
    temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));
    if(temperr) errno=temperr;
cache_remove_cleanup_4:
    temperr=pthread_mutex_unlock(&(aux2->node_mtx));
    if(temperr) errno=temperr;
    temperr=pthread_mutex_unlock(&(aux1->node_mtx));
    if(temperr) errno=temperr;
    return NULL;
}


// returns address and index of the node searched (if present)
static unsigned int lru_cache_addr_lookup(lru_cache* cache, const char* filename, conc_node* node) {
    if(!cache || !filename) {errno=EINVAL; return ERR;}
    if(!(cache->ht) || !(cache->queue)) {errno=EINVAL; return ERR;}

    conc_hash_table* ht=cache->ht;
    
    int temperr;
    unsigned int idx=conc_hash_hashfun(filename, (ht->size));
    while(1) {
        temperr=pthread_mutex_lock(&(((ht->table)[idx]).entry_mtx));
        if (temperr) {errno=temperr; return ERR;}

        // if we encounter a NULL value, then the element is not cached
        if(((ht->table)[idx]).entry==NULL) {errno=ENOENT; break;}
        // if we encounter a deleted element, we can go on to the next cycle
        else if(((ht->table)[idx]).entry==ht->mark);
        // if we find the element, we can return it
        else if(lru_cache_is_file_name(((ht->table)[idx].entry), filename)) {
            printf("TROVATOOOOOOOOOO\n\n");
            *node=((ht->table)[idx]).entry;
            break;
        }
        // if we encountered the wrong element, continue probing
        temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));
        if (temperr) {errno=temperr; return ERR;}
        idx++;
        idx=idx%(ht->size*2);
    }

    temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));
    if (temperr) {errno=temperr; return ERR;}

    return idx;        // returns either the file or NULL if the lookup failed
}


// returns TRUE if the element is already present in the hashtable, FALSE otherwise
static int lru_cache_is_duplicate(conc_node elem_a, conc_node elem_b) {
    if(!elem_a || !elem_b) {errno=EINVAL; return ERR;}
    if(!(strcmp(((file*)elem_a->data)->name, ((file*)elem_b->data)->name))) return TRUE;

    return FALSE;
}


// returns TRUE if the name of the file is the string given, FALSE otherwise
static int lru_cache_is_file_name(conc_node elem_a, const char* filename) {
    if(!elem_a || !filename) {errno=EINVAL; return ERR;}
    printf("\nCURR_FILE_NAME: %s\t FILENAME: %s\n", (((file*)elem_a->data)->name), filename);
    if(!(strcmp(((file*)elem_a->data)->name, filename))) return TRUE;

    return FALSE;
}


// returns a file struct constructed by the arguments given
file* lru_cache_build_file(char* name, int file_size, byte* data, byte f_lock, byte f_open, byte f_write) {
    if(!name || !data) {errno=EINVAL; return NULL;}

    file* newfile=(file*)malloc(sizeof(file));
    if(!newfile) return NULL;

    newfile->name=(char*)malloc((strlen(name))*sizeof(char));
    if(!(newfile->name)) {free(newfile); return NULL;}
    strcpy((newfile->name), name);

    newfile->file_size=file_size;
    newfile->data=data;
    newfile->f_lock=f_lock;
    newfile->f_open=f_open;
    newfile->f_write=f_write;

    return newfile;
}







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
    lru_cache* cache=(lru_cache*)arg;
    int i;
    printf("Thread %d starting\n", (gettid()));
    for(i=0; i<50; i++) {
        byte* num=(byte*)malloc(sizeof(byte));
        *num=i;
        file* file=lru_cache_build_file(strings[i], i+1, num, 0, 0, 0);
        lru_cache_push(cache, file);
        printf("Thread %d pushed\n", (gettid()));
    }

    return (void*) NULL;
}

void* popfun(void* arg) {
    lru_cache* cache=(lru_cache*)arg;
    int i;
    file* file;
    printf("Thread popper %d starting\n", (gettid()));
    for(i=0; i<50; i++) {
        file=lru_cache_pop(cache);
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