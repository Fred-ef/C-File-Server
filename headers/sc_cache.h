/* DESCRIZIONE CACHE */

#ifndef sc_cache_h
#define sc_cache_h

#include "conc_elem.h"
#include "conc_hash.h"
#include "conc_fifo.h"
#include "linkedlist.h"
#include "client_server_comm.h"

typedef enum {PROBING=-2, FOUND=0} search_result;

byte nth_chance;      // indicates the "chance" order of the algorithm (2 for second chance)

// defining the "second chance cache" structure
typedef struct sc_cache {
    conc_hash_table* ht;
    unsigned long max_file_number;
    unsigned long max_byte_size;
    unsigned long curr_file_number;
    unsigned long curr_byte_size;
    pthread_mutex_t mem_check_mtx;
} sc_cache;

// defining the structure of a file inside the cache
typedef struct file {
    char* name;     // represents the file's identifier inside the file server
    // HUFFMAN TREE CONTAINING THE DECOMPRESSION INFORMATION
    size_t file_size;
    byte* data;     // represents the actual information contained in the file
    byte f_lock;     // flag indicating if and by whom the file is currently locked
    byte f_open;     // flag indicating if the file is currently open
    byte f_write;    // flag indicating if the writeFile operation can be executed
    llist* open_list;     // holds a list of the users by which the file is currenlty opened
} file;


// MAIN OPERATIONS
sc_cache* sc_cache_create(const int, const int);                // returns an empty sc-cache data structure of capacity and size(bytes) given
int sc_cache_insert(sc_cache*, const file*, file***);      // pushes a file in the cache, as a "recently used" file, getting the expelled files
int sc_algorithm(sc_cache*, const size_t, file***, const bool, const char*);         // second chance replacement algorithm
int sc_lookup(sc_cache*, const char*, const op_code, const int*, byte**, const byte*, size_t*, file***);      // executes the request specified in the operation code
int sc_return_n_files(sc_cache*, const int, file***);   // returns the first N files saved on the cache
file* file_create(const char* pathname);      // returns an empty file with pathname as its name

#endif // sc_cache_h