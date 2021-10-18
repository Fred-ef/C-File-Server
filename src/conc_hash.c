#include "conc_hash.h"


conc_hash_table* conc_hash_create(int size) {
    conc_hash_table* ht=(conc_hash_table*)malloc(sizeof(conc_hash_table));     // allocates a hash table struct
    if(!ht) goto hash_create_cleanup;

    ht->table=(conc_hash_entry*)malloc(2*size*sizeof(conc_hash_entry));       // allocates a hash table of size "size"
    if(!(ht->table)) goto hash_create_cleanup;

    ht->mark=(conc_node)malloc(sizeof(generic_node_t));     // allocates an address for the "marker" element
    if(!(ht->mark)) goto hash_create_cleanup;

    ht->size=size;      // memorizes the table's size inside its struct

    int i;      // for loop index, temp err var
    // initializing each entry's mutex
    for(i=0; i<2*size; i++) {
        ((ht->table)[i]).entry=NULL;
        if((errno=pthread_mutex_init(&(((ht->table)[i]).entry_mtx), NULL)))
                goto hash_create_cleanup;
    }

    return ht;

hash_create_cleanup:
    cleanup_pointers(ht, (ht->table), (ht->mark), NULL);
    return NULL;
}

// calculates a hashing value of the string passed as parameter
unsigned int conc_hash_hashfun(const char* key, int size) {
    unsigned int i, result=0;
    for(i=0; i<(strlen(key)); i++) {
        result = key[i] + (result << 6) + (result << 16) - result;
    }
    printf("THREAD: %d\tKEY: %u\n", (gettid()), ((result%(get_next_prime(2*size))) % (2*size)));
    return ((result%(get_next_prime(2*size))) % (2*size));
}


// calculates the first prime number after the given number
int get_next_prime(int n) {
    int i, j, p=0;

    for(i=n; p==0; i++) {
        for(j=2; j<i; j++) {
            if((i%j)==0) break;
        }
        if(i==j) p=j;
    }

    return p;
}