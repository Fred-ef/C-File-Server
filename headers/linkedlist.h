// DESCRIZIONE

#ifndef linkedlist_h
#define linkedlist_h

#include "conc_elem.h"

typedef struct llist {
    conc_node head;     // list's head
} llist;

llist* ll_create(void*);    // Creates and returns an empty list, with arg as head's data
int ll_insert_head(llist*, void*);      // inserts a generic nodeat the head of the list
int ll_remove(llist*, void*);       // removes the specified element from the list
int ll_search(llist*, void*);       // returns true if the list contains the specified element
int ll_isEmpty(llist*);     // returns true if the list is empty
int ll_dealloc_full(llist*);        // deallocates the list with each node's data

#endif // linkedlist_h