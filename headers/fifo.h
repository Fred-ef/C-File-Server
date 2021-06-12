/*  This is an implementation of a "generic" type of FIFO queue, based on the
    "linkedlist.h" implementation of a "generic" type of linked list; the
    elements of the queue are of the same type of the elements of the linkedlist
    and support the same void-pointer representation of their data */

#ifndef fifo_h     // Multiple include protection
#define fifo_h

#include "linkedlist.h"     // Including the linkedlist library, which constitutes the backbone of this queue implementation

typedef generic_node_t qelem_t;
typedef qelem_t** head_ptr;
typedef qelem_t* node_t_ptr;

void* fifo_pop(head_ptr);     // Pops the element at head of the queue, returning its value (as a void pointer)
void fifo_push(head_ptr, void*);     // Pushes the element into the queue at the tail position
bool fifo_isEmpty(node_t_ptr);     // Returns TRUE if the queue is empty, otherwise FALSE
void fifo_dealloc_full(head_ptr);     // Deallocates each generic node of the queue and all of their data
void fifo_dealloc(head_ptr);     // Deallocates each generic node of the queue keeping all of their data
short fifo_position(head_ptr, void*);     // Returns the position of the argument inside the queue
size_t fifo_length(node_t_ptr);     // Returns the length of the queue

#endif // fifo_h