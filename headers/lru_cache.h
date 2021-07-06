/* DESCRIZIONE CACHE */

#ifndef lru_cache_h
#define lru_cache_h

#include "conc_elem.h"
#include "conc_fifo.h"
#include "conc_hash.h"

typedef struct lru_cache {
    conc_queue* queue;
    conc_hash_table* ht;
} lru_cache;


lru_cache* lru_cache_create(int);      // returns an empty lru-cache data structure of size "size"

#endif // lru_cache_h