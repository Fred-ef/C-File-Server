/* DESCRIZIONE ELEMENTO GENERICO CONCORRENTE */

#ifndef conc_elem_h
#define conc_elem_h

#include <pthread.h>
#include <errno.h>
#include <stdlib.h>

typedef struct generic_node_t {
  void* data;
  struct generic_node_t* next;
  pthread_mutex_t node_mtx;
} generic_node_t;

typedef generic_node_t* conc_node;

conc_node conc_node_create(void*);     // returns a generic concurrent ready-to-use node
void* conc_node_destroy(conc_node);     // deallocates a generic concurrent node, returning its data

#endif // conc_elem_h