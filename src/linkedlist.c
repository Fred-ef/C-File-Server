// TODO: sostituire con l'implementazione della funzione di cleanup dei puntatori

#include "linkedlist.h"

// Inserts a generic node at the head of the list
void ll_insertHead(head_ptr head, void* data) {
  if(head==NULL) return;     // Uninitialized list

  node_t_ptr newelement=(node_t_ptr)malloc(sizeof(generic_node_t));     // Creation of the new head-to-be
  newelement->data=data;     // Initializes the data of the new element
  newelement->next=NULL;     // Temporarily equates the next element of the new element to NULL value

  newelement->next=*head;     // Link new element with previous head (might be NULL)
  *head=newelement;     // link current head with new element
}

// Inserts a generic node at the tail of the list
void ll_insertTail(head_ptr head, void* data) {
  if(head==NULL) return;     // Uninitialized list

  node_t_ptr newelement=(node_t_ptr)malloc(sizeof(generic_node_t));     // Creation of the new tail-to-be
  newelement->data=data;     // Initializes the data of the new element
  newelement->next=NULL;     // Temporarily equates the next element of the new element to NULL value

  if((*head)==NULL) *head=newelement;     // If the list has no elements, place the element in head position
  else {
    node_t_ptr aux=NULL;     // Declaration of an auxiliary pointer that helps navigate the list
    for(aux=(*head); aux->next!=NULL; aux=aux->next);     // Iterates through the list till the last element (tail)
    aux->next=newelement;     // Links the old tail to the new tail
  }
}

// Removes the generic node at the list's head
void ll_removeHead(head_ptr head) {
  if(head==NULL) return;     // Uninitialized list

  if((*head)==NULL) return;     // List already empty
  else {
    node_t_ptr temp=*head;     // Saves current head in temp elem
    *head=(*head)->next;     // Replaces the old head with its successor (might be NULL)
    free(temp->data);     // Deallocates the old head's data
    free(temp);     // Deallocates the old head
  }
}

// Removes the generic node at the list's head without deallocating its data
void ll_popHead(head_ptr head) {
  if(head==NULL) return;    // Uninitialized list

  if((*head)==NULL) return;   // List already empty
  else *head=(*head)->next;    // Replaces the old head with its successor (might be NULL)
}

// Removes the generic node at the list's tail
void ll_removeTail(head_ptr head) {
  if(head==NULL) return;     // Uninitialized list

  if((*head)==NULL) return;     // List already empty
  else {
    node_t_ptr aux=NULL, temp=NULL;     // Declaration of two auxiliary pointers: one that helps navigate the list, the other holding the previous element
    for(aux=(*head), temp=(*head); aux->next!=NULL; aux=aux->next) temp=aux;     // Iterates through the list till the last element (tail)
    temp->next=NULL;     // Equates the successor of the new tail to NULL value
    free(aux->data);     // Deallocates the old tail's data
    free(aux);     // Deallocates the old tail
  }
}

// Removes the generic node from the list
void ll_remove(head_ptr head, void* data) {
  if(head==NULL) return;      // Uninitialized list
  if((*head)==NULL) return;     // Empty list

  if((*head)->data == data) { (*head)=(*head)->next; return; }      // If the element is the head, remove it

  node_t_ptr aux=NULL, temp=NULL;     // Auxiliary pointers
  for(aux=temp=(*head); aux!=NULL; aux=aux->next) {
    if((aux->data)==data) { temp->next = aux->next; return; }     // If the element is found, return it
    else temp=aux;      // Update the auxiliary pointer
  }

  return;     // The element was not in the list
}

// Returns TRUE if the list is empty, ELSE otherwise
bool ll_isEmpty(node_t_ptr head) {
  if(head==NULL) return TRUE;
  return FALSE;
}

// Returns the position of the element inside the queue
short ll_position(head_ptr head, void* data) {
  if(head==NULL) return -1;      // Uninitialized list
  if((*head)==NULL) return -1;     // Empty list

  node_t_ptr aux=NULL, temp=NULL;
  unsigned short i=0;     // loop index
  for(aux=temp=(*head); aux!=NULL; aux=aux->next, i++) {
    if((aux->data)==data) return i;
    else temp=aux;
  }

  return -1;     // The element has not been found
}

// Deallocates each generic node of the list and all of their data
void ll_dealloc_full(head_ptr head) {
  if(head==NULL) return;     // Uninitialized list

  if((*head)==NULL) return;     // List already empty
  else {
    node_t_ptr aux=NULL, next=NULL;     // Declaration of two auxiliary pointers: one that helps navigate the list, the other holding the first's successor
    for(aux=(*head); aux!=NULL; aux=next) {     // Starts at the head, continues to navigate the list at each iteration
      next=aux->next;     // Temporarily memorized the successor of the current element (might be NULL)
      free(aux->data);     // Deallocates the data held by the current element
      free(aux);     // Deallocates the current element
    }
    *head=NULL;
  }
}

// Deallocates each generic node of the list keeping all of their data
void ll_dealloc(head_ptr head) {
  if(head==NULL) return;     // Uninitialized list

  if((*head)==NULL) return;     // List already empty
  else {
    node_t_ptr aux=NULL, next=NULL;     // Declaration of two auxiliary pointers: one that helps navigate the list, the other holding the first's successor
    for(aux=(*head); aux!=NULL; aux=next) {     // Starts at the head, continues to navigate the list at each iteration
      next=aux->next;     // Temporarily memorized the successor of the current element (might be NULL)
      free(aux);     // Deallocates the current element
    }
    *head=NULL;
  }
}

// Returns the length of the list
size_t ll_length(node_t_ptr head) {
  if(head==NULL) return 0;
  else {
    node_t_ptr aux=NULL;     // Declaration of an auxiliary pointer that helps navigate the list
    size_t counter=0;     // Declaration of a size_t (int) variable used to count the elements in the list
    for(aux=head; aux!=NULL; aux=aux->next, counter++);     // Increases the counter variable at each iteration
    return counter;     // Returns the stored length
  }
}