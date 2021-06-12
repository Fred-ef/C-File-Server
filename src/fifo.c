#include "fifo.h"

// Pops the element at head of the queue, returning its value (as a void pointer)
void* fifo_pop(head_ptr q) {
  if(q==NULL) return (void*)NULL;     // Queue not initialized

  void* tempData=NULL;
  if(*q!=NULL) tempData=(*q)->data;
  ll_popHead(q);
  return tempData;
}

// Pushes the element into the queue at the tail position
void fifo_push(head_ptr q, void* data) {
  if(q==NULL) return;     // Queue not initialized

  ll_insertTail(q, data);     // Calls the insertTail procedure defined on linkedlist.h
}

// Returns TRUE if the queue is empty, otherwise FALSE
bool fifo_isEmpty(node_t_ptr q) {
  return ll_isEmpty(q);
}

// Returns the position of data inside q
short fifo_position(head_ptr q, void* data) {
  return ll_position(q, data);
}

// Deallocates each generic node of the queue and all of their data
void fifo_dealloc_full(head_ptr q) {
  ll_dealloc_full(q);
}

// Deallocates each generic node of the queue keeping all of their data
void fifo_dealloc(head_ptr q) {
  ll_dealloc(q);
}

// Returns the length of the queue
size_t fifo_length(node_t_ptr q) {
  return ll_length(q);
}