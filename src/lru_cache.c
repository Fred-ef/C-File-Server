#include "lru_cache.h"


lru_cache* lru_cache_create(int size) {
    lru_cache* cache=(lru_cache*)malloc(sizeof(lru_cache));
    if(!cache) goto cache_create_cleanup;

    cache->queue=conc_fifo_create(NULL);
    if(!(cache->queue)) goto cache_create_cleanup;
    cache->ht=conc_hash_create(size);
    if(!(cache->ht)) goto cache_create_cleanup;;

    return cache;

cache_create_cleanup:
    cleanup_pointers(cache, (cache->queue), (cache->ht), NULL);
    return NULL;
}