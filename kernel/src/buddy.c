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

    uintptr_t heap_real_start=ROUNDUP(HEAP_START+kernel_max,Unit_size),heap_real_end=ROUNDDOWN(HEAP_END,Unit_size);
    printf("%lx %lx %lx\n",heap_real_start,heap_real_end,HEAP_OFFSET_START);
    printf("%lx %lx\n",Unit_size*size,size);
    for(size_t i=size,j=HEAP_OFFSET_START;i<2*size;i++,j+=Unit_size){
        printf("%lx\n",j);
        if(j>=heap_real_start&&j<heap_real_end){
            self->longest[i]=1;
            printf("%lx-%p\n",i,(void *)j);
        }
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

    uintptr_t index=1;
    for(int node_size=self->size;node_size>size;node_size/=2){
        size_t lch_longest=self->longest[lch(index)],rch_longest=self->longest[rch(index)];
        if(lch_longest>=size&&(lch_longest<rch_longest||rch_longest<size)) index=lch(index);
        else index=rch(index);
    }

    self->longest[index]=0;
    uintptr_t offset=index*size-self->size;

    for(index=fa(index);index;index=fa(index)) 
    self->longest[index]=Max(self->longest[lch(index)],self->longest[rch(index)]);

    void * ret=(void *)(HEAP_OFFSET_START+offset*Unit_size);
    DEBUG(memset(ret,MAGIC_BIG,size);)
    return ret;
}

void buddy_free(buddy * self,void * ptr){
    size_t node_size=1,offset=((uintptr_t)ptr-HEAP_OFFSET_START)/Unit_size;
    uintptr_t index=self->size+offset;
    for(;index&&self->longest[index];index=fa(index)) node_size<<=1;
    Assert(index,"HAS NOT BEEN ALLOCATED! %p\n",(void *)offset);

    self->longest[index]=node_size;
    DEBUG(memset(ptr,MAGIC_UNUSED,node_size*Unit_size);)

    for(index=fa(index),node_size<<=1;index;index=fa(index),node_size<<=1){
        size_t lch_longest=self->longest[lch(index)],rch_longest=self->longest[rch(index)];
        if(lch_longest+rch_longest==node_size) self->longest[index]=node_size;
        else self->longest[index]=Max(lch_longest,rch_longest);
    }
}

unsigned char is_block(buddy *self,size_t offset){
    return self->longest[self->size+offset]==0;
}