#include "conc_fifo.h"


int conc_fifo_push(conc_queue* queue, void* data) {
    if(!queue) {errno=EINVAL; return ERR;}     // Uninitialized list
    if(!(queue->head)) {errno=EINVAL; return ERR;}     // Uninitialized list

    int temperr;
    conc_node newelement=(conc_node)malloc(sizeof(generic_node_t));     // Creation of the new tail-to-be
    if(!newelement) return ERR;     // errno already set
    newelement->data=data;     // Initializes the data of the new element
    newelement->next=NULL;     // Temporarily equates the next element of the new element to NULL value
    temperr=pthread_mutex_init(&(newelement->node_mtx), NULL);      // Initializes new element's mutex
    if(temperr) {errno=temperr; free(newelement); return ERR;}

    temperr=pthread_mutex_lock(&((queue->head)->node_mtx));
    if(temperr) {errno=temperr; free(newelement); return ERR;}

    if(!((queue->head)->next)) {         // If list is empty, append element and return
        (queue->head)->next=newelement;
        temperr=pthread_mutex_unlock(&((queue->head)->node_mtx));
        if(temperr) {errno=temperr; free(newelement); return ERR;}
        return SUCCESS;
    }     
    else {
        conc_node aux1=queue->head, aux2;
        while(aux1->next!=NULL) {
            aux2=aux1;
            aux1=aux1->next;
            temperr=pthread_mutex_lock(&(aux1->node_mtx));
            if(temperr) {errno=temperr; free(newelement); return ERR;}
            temperr=pthread_mutex_unlock(&(aux2->node_mtx));
        }
        aux1->next=newelement;
        temperr=pthread_mutex_unlock(&(aux1->node_mtx));
        if(temperr) {errno=temperr; free(newelement); return ERR;}
    }
    return SUCCESS;
}


// Removes the generic node at the list's head
void* conc_fifo_pop(conc_queue* queue) {
    if(!queue) {errno=EINVAL; return NULL;}     // Uninitialized list
    if(!(queue->head)) {errno=EINVAL; return NULL;}     // Uninitialized list

    int temperr;
    void* tempdata=NULL;

    temperr=pthread_mutex_lock(&((queue->head)->node_mtx));
    if(temperr) {errno=temperr; return NULL;}

    if(!((queue->head)->next)) {errno=EINVAL; return NULL;}

    conc_node aux=(queue->head)->next;
    temperr=pthread_mutex_lock(&(aux->node_mtx));
    if(temperr) {errno=temperr; return NULL;}

    (queue->head)->next=aux->next;
    tempdata=aux->data;
    aux->next=NULL;
    temperr=pthread_mutex_unlock(&((queue->head)->node_mtx));
    if(temperr) {errno=temperr; return NULL;}
    temperr=pthread_mutex_unlock(&(aux->node_mtx));
    if(temperr) {errno=temperr; return NULL;}
    temperr=pthread_mutex_destroy(&(aux->node_mtx));
    if(temperr) {errno=temperr; return NULL;}

    free(aux);
    return tempdata;
}

// Returns TRUE if the list is empty, ELSE otherwise
int conc_fifo_isEmpty(conc_queue* queue) {
  if(!queue) {errno=EINVAL; return ERR;}     // Uninitialized list
  if(!(queue->head)) {errno=EINVAL; return ERR;}     // Uninitialized list

  int temperr;
  temperr=pthread_mutex_lock(&((queue->head)->node_mtx));
  if(temperr) {errno=temperr; return ERR;}
  if(!((queue->head)->next)) return TRUE;

  return FALSE;
}


// Deallocates each generic node of the list and all of their data; WARNING: NON CONCURRENT
int fifo_dealloc_full(conc_queue* queue) {
    if(!queue) {errno=EINVAL; return ERR;}     // Uninitialized list
    if(!(queue->head)) {errno=EINVAL; return ERR;}     // Uninitialized list

    int temperr;
    
    if(!((queue->head)->next)) {        // For an empty list, deallocating its head node and pointer suffices
        temperr=pthread_mutex_destroy(&((queue->head)->node_mtx));
        if(temperr) {errno=temperr; return ERR;}
        free((queue->head)->data);
        free(queue->head);
        free(queue);
        return SUCCESS;
    }

    conc_node aux1=queue->head, aux2;
    while(aux1!=NULL) {
        aux2=aux1;
        aux1=aux1->next;
        temperr=pthread_mutex_destroy(&((aux2)->node_mtx));
        if(temperr) {errno=temperr; return ERR;}
        free((aux2)->data);
        free(aux2);
    }

    free(queue);
    return SUCCESS;
}