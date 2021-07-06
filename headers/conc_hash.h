/* DESCRIZIONE TABELLA HASH CONCORRENTE */

#ifndef conc_hash_h
#define conc_hash_h

#include <stdint.h>

#include "conc_elem.h"

typedef struct conc_hash_table {
    conc_node* table;
    conc_node mark;
    int size;
} conc_hash_table;

typedef conc_node* hash_ptr;

conc_hash_table* conc_hash_create(int);      // Creates and returns an empty concurrent hashtable
int conc_hash_hashfun(uintptr_t, int);      // Calculate a hash value for the operations
int conc_hash_insert(conc_hash_table*, conc_node);        // Inserts an allocated concurrent node in the hash table

#endif // conc_hash_h