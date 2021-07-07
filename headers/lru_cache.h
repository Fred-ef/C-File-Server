/* DESCRIZIONE CACHE */

#ifndef lru_cache_h
#define lru_cache_h

#include <string.h>

#include "conc_elem.h"
#include "conc_fifo.h"
#include "conc_hash.h"

typedef struct lru_cache {
    conc_queue* queue;
    conc_hash_table* ht;
} lru_cache;

typedef struct file {
    char* name;     // represents the file's identifier inside the file server
    // HUFFMAN TREE CONTAINING THE DECOMPRESSION INFORMATION
    byte* data;     // represents the actual information contained in the file
    byte f_lock;     // flag indicating if and by whom the file is currently locked
    byte f_open;     // flag indicating if the file is currently open
    byte f_write;    // flag indicating if the writeFile operation can be executed
    // LOCK QUEUE
    // OPEN LIST
} file;


lru_cache* lru_cache_create(int);      // returns an empty lru-cache data structure of size "size"
int lru_cache_push(lru_cache*, file*);        // pushes a file in the cache, as the "most recently used" file
file* lru_cache_pop(lru_cache*);        // pops the LRU file from the cache and returns it
static int conc_hash_is_duplicate(conc_node, conc_node);

file* lru_cache_build_file(char*, byte*, byte, byte, byte);     // returns a file built from the arguments passed

#endif // lru_cache_h