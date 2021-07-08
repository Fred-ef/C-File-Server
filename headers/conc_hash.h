/* DESCRIZIONE TABELLA HASH CONCORRENTE */

#ifndef conc_hash_h
#define conc_hash_h

#include <stdint.h>
#include <string.h>
#include <math.h>

#include "conc_elem.h"

typedef struct conc_hash_entry {
    conc_node entry;
    pthread_mutex_t entry_mtx;
} conc_hash_entry;

typedef struct conc_hash_table {
    conc_hash_entry* table;
    conc_node mark;
    int size;
} conc_hash_table;

conc_hash_table* conc_hash_create(int);      // Creates and returns an empty concurrent hashtable
unsigned int conc_hash_hashfun(const char*, int);       // Calculate a hash value for the operations

int get_next_prime(int);     // returns the first prime number after the argument passed

#endif // conc_hash_h