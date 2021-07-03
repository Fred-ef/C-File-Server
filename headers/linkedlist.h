/*  This is an implementation of a "generic" type of list, whose node are
    of type generic_node_t, a type designed to carry whatever type of
    information thanks to a void pointer implementing its data content */

#ifndef likedlist_h     // Multiple include protection
#define linkedlist_h

#include <stdlib.h>

#include "definitions.h"

typedef struct generic_node_t {
  void* data;
  struct generic_node_t* next;
  struct generic_node_t* prev;
} generic_node_t;

typedef generic_node_t** head_ptr;
typedef generic_node_t* node_t_ptr;

void ll_insertHead(head_ptr, void*);     // Inserts a generic node at the head of the list
void ll_insertTail(head_ptr, void*);     // Inserts a generic node at the tail of the list
void ll_removeHead(head_ptr);     // Removes the generic node at the list's head
void ll_popHead(head_ptr);    // Removes the geniric node at the list's head without deallocating data
void ll_removeTail(head_ptr);     //Removes the generic node at the list's tail
void ll_remove(head_ptr, void*);      // Removes the generic node from the list
bool ll_isEmpty(node_t_ptr);     // Returns TRUE if the list is empty, ELSE otherwise
short ll_position(head_ptr, void*);      // Returns the position of the element inside the list
void ll_dealloc_full(head_ptr);     // Deallocates each generic node of the list and all of their data
void ll_dealloc(head_ptr);     // Deallocates each generic node of the list keeping all of their data
size_t ll_length(node_t_ptr);     // Returns the length of the list

#endif // linkedlist_h