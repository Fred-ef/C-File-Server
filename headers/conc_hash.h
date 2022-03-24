/* DESCRIZIONE TABELLA HASH CONCORRENTE */

#ifndef conc_hash_h
#define conc_hash_h

#include <stdint.h>
#include <string.h>
#include <math.h>

#include "conc_elem.h"

typedef struct conc_hash_entry {
    void* entry;        // the element held in the current slot
    pthread_mutex_t entry_mtx;      // mutex for synchronizing the single element
    bool r_used;    // flag indicating if the entry has been recently used
} conc_hash_entry;

typedef struct conc_hash_table {
    conc_hash_entry* table;     // Pointer to the hash table (acts as its head)
    conc_node mark;     // special-value element that will be used as a marker
    int size;       // holds the size of the hash table
} conc_hash_table;

conc_hash_table* conc_hash_create(int);      // Creates and returns an empty concurrent hashtable
unsigned int conc_hash_hashfun(const char*, int);       // Calculate a hash value for the operations
int hash_dealloc_full(conc_hash_table*);    // deallocates the whole hash table WARNING non concurrent


int get_next_prime(int);     // returns the first prime number after the argument passed

#endif // conc_hash_h