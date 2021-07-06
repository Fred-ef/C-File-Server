#include "conc_hash.h"

static int hash_param=0;


conc_hash_table* conc_hash_create(int size) {
    conc_hash_table* ht=(conc_hash_table*)malloc(sizeof(conc_hash_table));     // allocates a hash table struct
    if(!ht) goto hash_create_cleanup;

    ht->table=(conc_node*)malloc(size*sizeof(conc_node));       // allocates a hash table of size "size"
    if(!(ht->table)) goto hash_create_cleanup;

    ht->mark=(conc_node)malloc(sizeof(generic_node_t));     // allocates an address for the "marker" element
    if(!(ht->mark)) goto hash_create_cleanup;

    ht->size=size;      // memorizes the table's size inside its struct

    int i;      // for loop index

    /*
    for(i=0; i<size; i++) {     // allocates each pointer in the table
        ht[i]=(conc_node)malloc(sizeof(generic_node_t));
        if(!(ht[i])) return NULL;
    }
    */

    // in our case, each element will already be allocated: no need to take up extra memory
    for(i=0; i<size; i++) {
        (ht->table)[i]=NULL;
    }

    return ht;

hash_create_cleanup:
    cleanup_pointers(ht, (ht->table), (ht->mark), NULL);
    return NULL;
}


int conc_hash_insert(conc_hash_table* ht, conc_node node) {
    if(!node) {errno=EINVAL; return ERR;}

    int idx=conc_hash_hashfun((uintptr_t)node, ht->size);
    while(((ht->table)[idx])!=NULL && ((ht->table)[idx])!=(ht->mark)) {
        idx++;
        idx%=(ht->size);
    }

    (ht->table)[idx]=node;

    return SUCCESS;
}


int conc_hash_hashfun(uintptr_t key, int size) {
    return ((key%(get_next_prime(size))) % size);
}


// calculates the first prime number after the given number
static int get_next_prime(int n) {
    int i, j, p=0;

    for(i=n; p=0; i++) {
        for(j=2; j<i; j++) {
            if((i%j)==0) break;
        }
        if(i==j) p=j;
    }

    return p;
}