/* DESCRIZIONE CODA CONCORRENTE */

#ifndef conc_fifo_h
#define conc_fifo_h

#include "conc_elem.h"

typedef struct conc_queue {
    conc_node head;     // pointer to the actual queue
    pthread_mutex_t queue_mtx;      // mutex to implement coarse-grained locking
    pthread_cond_t queue_cv;        // cond var to let threads wait on conditions related to the queue (to use with coarse-grained locking)
} conc_queue;

conc_queue* conc_fifo_create(void*);      // Creates and returns an empty concurrent queue, with arg as head's data
int conc_fifo_push(conc_queue*, void*);     // Inserts a generic node at the tail of the list
void* conc_fifo_pop(conc_queue*);     // Removes the generic node at the list's head
int conc_fifo_isEmpty(conc_queue*);     // Returns TRUE if the list is empty, FALSE otherwise
int fifo_dealloc_full(conc_queue*);     // Deallocates each generic node of the list and all of their data

#endif // conc_fifo_h