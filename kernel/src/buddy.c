#include <common.h>
#include "pmm.h"

static inline size_t upp(size_t x){
    size_t i=1;
    for(;i<x;i<<=1);
    return i;
}

#define lch(x) ((x)<<1)
#define rch(x) ((x)<<1|1)
#define fa(x) ((x)>>1)

buddy * buddy_init(size_t size){
    size=upp(size);
    buddy * self=(buddy *)kernel_alloc(2*size*sizeof(int));
    self->size=size;

    uintptr_t heap_real_start=ROUNDUP(HEAP_START+kernel_alloc,Unit_size),heap_real_end=ROUNDDOWN(HEAP_END,Unit_size);
    for(size_t i=size,j=HEAP_OFFSET_START;i<2*size;i++,j+=Unit_size){
        if(j>=heap_real_start&&j<heap_real_end) self->longest[i]=1;
        else self->longest[i]=0;
    }

    size_t len=2;
    for(size_t i=size-1;i>=1;i--) {
        size_t lch_longest=self->longest[lch(i)],rch_longest=self->longest[rch(i)];
        self->longest[i]=(lch_longest+rch_longest==len?len:Max(lch_longest,rch_longest));
        if(LOWBIT(i)==i) len<<=1;
    }
    return self;
}

void * buddy_alloc(buddy * self,size_t size){
    size=upp(size);
    if(self->longest[1]<size) return NULL;//cannot find available!

    uintptr_t index=1,offset=0;
    for(int node_size=self->size;node_size>size;node_size/=2){
        size_t lch_longest=self->longest[lch(index)],rch_longest=self->longest[rch(index)];
        if(lch_longest>=size&&(lch_longest<rch_longest||rch_longest<size)) index=lch(index);
        else index=rch(index);
    }

    self->longest[index]=0;offset=index*size-self->size;

    for(index=fa(index);index;index=fa(index)) 
    self->longest[index]=Max(self->longest[lch(index)],self->longest[rch(index)]);

    return (void *)(HEAP_OFFSET_START+index*Unit_size);
}

void buddy_free(buddy * self,size_t offset){
    size_t node_size=1;
    uintptr_t index=self->size+offset;
    for(;index&&self->longest[index];index=fa(index)) node_size<<=1;
    Assert(index,"HAS NOT BEEN ALLOCATED! %p\n",(void *)offset);

    self->longest[index]=node_size;

    for(index=fa(index),node_size<<=1;index;index=fa(index),node_size<<=1){
        size_t lch_longest=self->longest[lch(index)],rch_longest=self->longest[rch(index)];
        if(lch_longest+rch_longest==node_size) self->longest[index]=node_size;
        else self->longest[index]=Max(lch_longest,rch_longest);
    }
}