/* DESCRIZIONE CACHE */

#ifndef lru_cache_h
#define lru_cache_h

#include "conc_elem.h"
#include "conc_fifo.h"
#include "conc_hash.h"

// defining the "lru cache" structure
typedef struct lru_cache {
    conc_queue* queue;
    conc_hash_table* ht;
    int max_file_number;
    int max_byte_size;
    int curr_file_number;
    int curr_byte_size;
    pthread_mutex_t mem_check_mtx;
} lru_cache;

// defining the structure of a file inside the cache
typedef struct file {
    char* name;     // represents the file's identifier inside the file server
    // HUFFMAN TREE CONTAINING THE DECOMPRESSION INFORMATION
    int file_size;
    byte* data;     // represents the actual information contained in the file
    byte f_lock;     // flag indicating if and by whom the file is currently locked
    byte f_open;     // flag indicating if the file is currently open
    byte f_write;    // flag indicating if the writeFile operation can be executed
    // LOCK QUEUE
    // OPEN LIST
} file;


// MAIN OPERATIONS
lru_cache* lru_cache_create(int, int);      // returns an empty lru-cache data structure of capacity and size(bytes) given
int lru_cache_push(lru_cache*, file*);        // pushes a file in the cache, as the "most recently used" file
file* lru_cache_pop(lru_cache*);        // pops the LRU file from the cache and returns it
file* lru_cache_lookup(lru_cache*, const char*);        // returns the file of the given name
file* lru_cache_remove(lru_cache*, const char*);        // removes the file of the given name and returns it

// HELPER FUNCTIONS
static unsigned int lru_cache_addr_lookup(lru_cache*, const char*, conc_node*);        // returns address and index of the node searched
static int lru_cache_is_duplicate(conc_node, conc_node);        // returns TRUE if the two elements are equal, FALSE otherwise
static int lru_cache_is_file_name(conc_node, const char*);      // returns TRUE if the name of the file is the string given
file* lru_cache_build_file(char*, int, byte*, byte, byte, byte);     // returns a file built from the arguments passed

#endif // lru_cache_h