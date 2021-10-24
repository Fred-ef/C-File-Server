#include "linkedlist.h"

llist* ll_create(void* data) {
    llist* list=(llist*)malloc(sizeof(llist));
    if(!list) return NULL;

    list->head==conc_node_create(data);
    if(!(list->head)) {free(list); return NULL;}

    return list;
}


// NOTE: returns without inserting if the element is already in the list
int ll_insert_head(llist* list, void* data) {
    if(!list) {errno=EINVAL; return ERR;}     // Uninitialized list
    if(!(list->head)) {errno=EINVAL; return ERR;}     // Uninitialized list
    if(!data) {errno=EINVAL; return ERR;}   // invalid data

    if(ll_search(list, data)) return SUCCESS;

    conc_node newelement=conc_node_create(data);
    if(!newelement) return ERR;     // errno already set

    if(!((list->head)->next)) {
        (list->head)->next=newelement;
    }
    else {
        newelement->next=(list->head)->next;
        (list->head)->next=newelement;
    }
    
    return SUCCESS;
}


// NOTE: returns TRUE if the element is NOT in the list
int ll_remove(llist* list, void* data) {
    if(!list) {errno=EINVAL; return ERR;}     // Uninitialized list
    if(!(list->head)) {errno=EINVAL; return ERR;}     // Uninitialized list
    if(!data) {errno=EINVAL; return ERR;}   // invalid data

    conc_node aux1=list->head, aux2;
    while(aux1!=NULL && ((int)aux1->data != (int)data)) {
        aux2=aux1;
        aux1=aux1->next;
    }

    if(aux1) {
        aux2->next=aux1->next;
        free(aux1);
    }

    return SUCCESS;
}


int ll_search(llist* list, void* data) {
    if(!list) {errno=EINVAL; return ERR;}     // Uninitialized list
    if(!(list->head)) {errno=EINVAL; return ERR;}     // Uninitialized list
    if(!data) {errno=EINVAL; return ERR;}   // invalid data

    conc_node aux1;
    for(aux1=list->head; (aux1!=NULL && ((int)aux1->data != (int)data)); aux1=aux1->next);

    if(!aux1) return FALSE;
    return TRUE;
}


int ll_isEmpty(llist* list) {
    if(!list) {errno=EINVAL; return ERR;}     // Uninitialized list
    if(!(list->head)) {errno=EINVAL; return ERR;}     // Uninitialized list

    if(!((list->head)->next)) return TRUE;
    else return FALSE;
}


// deallocates the full list
int ll_dealloc_full(llist* list) {
    if(!list) {errno=EINVAL; return ERR;}   // Uninitialized list

    if(!(list->head)) free(list);
    else {
        conc_node aux1=list->head, aux2;
        while(aux1!=NULL) {
            aux2=aux1;
            aux1=aux1->next;
            free(aux2);
        }
    }

    free(list);
    return SUCCESS;
}