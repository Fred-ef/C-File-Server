#include "conc_elem.h"

conc_node conc_node_create(void* data) {
    conc_node newnode=(conc_node)malloc(sizeof(generic_node_t));
    if(!newnode) return NULL;

    int temperr;
    newnode->data=data;
    newnode->next=NULL;
    temperr=pthread_mutex_init(&(newnode->node_mtx), NULL);
    if(temperr) {errno=temperr; return NULL;}

    return newnode;
}

void* conc_node_destroy(conc_node node) {
    if(!node) {errno=EINVAL; return NULL;}      // non-allocated node

    int temperr;
    void* tempdata;

    tempdata=node->data;
    node->next=NULL;
    temperr=pthread_mutex_destroy(&(node->node_mtx));
    if(temperr) {errno=temperr; return NULL;}
    free(node);

    return (void*)tempdata;
}