#include <common.h>
#include "macros_for_pmm.h"
#include "buddy.h"

static inline size_t upp(size_t x){
    size_t i=1;
    for(;i<x;i<<=1);
    return i;
}

buddy * buddy_init(size_t size){
    buddy * self;
    self=(buddy *)kernel_alloc(2*upp(size)*sizeof(int));
    self->size=size;

    int node_size=size * 2;
    for(int i=0;i<2*size-1;++i){
        if(LOWBIT(i+1)==(i+1)) node_size>>=1;
        self->longest[i]=node_size;
    }

    return self;
}

int buddy_alloc(buddy * self,int size){
    size=upp(size);
    int index=0,offset=0;
    for(int node_size=self->size;node_size!=size;)
}

void buddy_free(buddy * self,int offset);