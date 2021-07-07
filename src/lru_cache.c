#include "lru_cache.h"


// returns an LRU-cache of the given size
lru_cache* lru_cache_create(int size) {
    lru_cache* cache=(lru_cache*)malloc(sizeof(lru_cache));
    if(!cache) goto cache_create_cleanup;

    cache->queue=conc_fifo_create(NULL);
    if(!(cache->queue)) goto cache_create_cleanup;
    cache->ht=conc_hash_create(size);
    if(!(cache->ht)) goto cache_create_cleanup;;

    return cache;

cache_create_cleanup:
    cleanup_pointers(cache, (cache->queue), (cache->ht), NULL);
    return NULL;
}


// pushes the given file inside the cache
int lru_cache_push(lru_cache* cache, file* file) {
    if(!cache || !file) {errno=EINVAL; return ERR;}
    if(!(cache->ht) || !(cache->queue)) {errno=EINVAL; return ERR;}

    conc_queue* queue=cache->queue;
    conc_hash_table* ht=cache->ht;

    int temperr, found=0;
    conc_node newelement=conc_node_create((void*)file);
    if(!newelement) return ERR;

    // first, find an entry in the hashtable and lock it
    int idx=conc_hash_hashfun((uintptr_t)newelement, (ht->size));
    while(!found) {
        // lock the hashtable entry
        temperr=pthread_mutex_lock(&(((ht->table)[idx]).entry_mtx));
        if (temperr) {errno=temperr; free(newelement); return ERR;}

        // if the hashtable entry is empty, select it to insert the element
        if(((ht->table)[idx]).entry==NULL || ((ht->table)[idx]).entry==(ht->mark)) {
            // insert the element in the hash table first
            ((ht->table)[idx]).entry=newelement;
            printf("PUSHED CORRECTLY\n\n");     // TODO REMOVE
            found=1;
        }

        // if the element is already cached, return an error
        else if((temperr=conc_hash_is_duplicate(((ht->table)[idx]).entry, newelement))) {
            if(temperr==TRUE) errno=EEXIST;
            else if(temperr==ERR) errno=EINVAL;
            temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));
            if (temperr) {errno=temperr; free(newelement); return ERR;}
            free(newelement); return ERR;
        }
        
        // if the entry was occupied by another element, continue probing
        else {
            printf("PROSSIMO ELEM\n");
            temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));
            if (temperr) {errno=temperr; free(newelement); return ERR;}
            idx++;
            idx=idx%(ht->size);
        }
    }

    temperr=pthread_mutex_lock(&((queue->head)->node_mtx));
    if(temperr) {errno=temperr; free(newelement); return ERR;}

    // then push the element into the queue as well
    if(!((queue->head)->next)) {        // if the list is EMPTY
        (queue->head)->next=newelement;
        temperr=pthread_mutex_unlock(&((queue->head)->node_mtx));
        if(temperr) {errno=temperr; free(newelement); return ERR;}
        temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));
        if (temperr) {errno=temperr; free(newelement); return ERR;}
        printf("INSERTED IN EMPTY LIST\n");        // TODO REMOVE
    }

    else{       //if the list is NOT EMPTY
        conc_node aux1=queue->head, aux2;
        while(aux1->next!=NULL) {       // scan untill the end of the queue
            aux2=aux1;
            aux1=aux1->next;
            temperr=pthread_mutex_lock(&(aux1->node_mtx));
            if(temperr) {errno=temperr; free(newelement); return ERR;}
            temperr=pthread_mutex_unlock(&(aux2->node_mtx));
            if(temperr) {errno=temperr; free(newelement); return ERR;}
        }
        // when at the tail, insert the element
        aux1->next=newelement;
        temperr=pthread_mutex_unlock(&(aux1->node_mtx));
        if(temperr) {errno=temperr; free(newelement); return ERR;}
        temperr=pthread_mutex_unlock(&(((ht->table)[idx]).entry_mtx));
        if (temperr) {errno=temperr; free(newelement); return ERR;}
        printf("INSERTED IN LIST\n");       // TODO REMOVE
    }

    return SUCCESS;
}


// pops the last-recently-used elements from the cache and returns it
file* lru_cache_pop(lru_cache* cache) {
    if(!cache) {errno=EINVAL; return NULL;}

    return NULL;
}


// returns TRUE if the element is already present in the hashtable, FALSE otherwise
static int conc_hash_is_duplicate(conc_node elem_a, conc_node elem_b) {
    if(!elem_a || !elem_b) {errno=EINVAL; return ERR;}
    if(!(strcmp(((file*)elem_a->data)->name, ((file*)elem_b->data)->name))) return TRUE;

    return FALSE;
}


// returns a file struct constructed by the arguments given
file* lru_cache_build_file(char* name, byte* data, byte f_lock, byte f_open, byte f_write) {
    if(!name || !data) {errno=EINVAL; return NULL;}

    file* newfile=(file*)malloc(sizeof(file));
    if(!newfile) return NULL;

    newfile->name=(char*)malloc((strlen(name))*sizeof(char));
    if(!(newfile->name)) {free(newfile); return NULL;}
    strcpy((newfile->name), name);
    
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
    int i, temperr;
    printf("Thread %d starting\n", (gettid()));
    for(i=0; i<50; i++) {
        byte* num=(byte*)malloc(sizeof(byte));
        *num=i;
        char str[] = { [41] = '\1' }; // make the last character non-zero so we can test based on it later
        rand_str(str, sizeof str - 1);
        file* file=lru_cache_build_file(str, num, 0, 0, 0);
        temperr=lru_cache_push(cache, file);
        printf("Thread %d pushed\n", (gettid()));
    }

    return (void*) NULL;
}

int main() {
    lru_cache* cache=lru_cache_create(500);
    if(!cache) return -1;
    pthread_t* tid_arr=(pthread_t*)malloc(10*sizeof(pthread_t));
    if(!tid_arr) return -1;
    int i;

    for(i=0; i<10; i++) {
        pthread_create(&(tid_arr[i]), NULL, pushfun, (void*)cache);
        printf("Spawned thread %d\n", i);
    }

    for(i=0; i<10; i++) {
        pthread_join(tid_arr[i], NULL);
        printf("Joined thread %d\n", i);
    }

    return 0;
}