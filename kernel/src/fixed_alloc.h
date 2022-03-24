#define __contact(x,y) x##y
#define contact(x,y) __contact(x,y)
#define head(x) contact(start_of_,x)
#define start(x) contact(heap_,contact(x,_start))
#define end(x) contact(heap_,contact(x,_end))
#define lock(x) contact(lock_,x)

#define CAO_FIXED_INIT(x,start_addr,len)\
  static free_list * head(x);\
  static uintptr_t start(x),end(x);\
  static int lock(x);\
  static inline void contact(init_,x)(){\
    start(x)=start_addr;end(x)=start_addr+(len);\
    head(x)=(free_list *)start(x);\
    head(x)->size=x;\
    for(uintptr_t ptr=start(x)+x;ptr<end(x);ptr+=x){\
      ((free_list *)ptr)->size=x;\
      ((free_list *)(ptr-x))->nxt=(free_list *)ptr;\
    }\
    ((free_list *)(end(x)-x))->nxt=NULL;\
    return;\
  }
#define CAO_ALLOC(x)\
  static inline void * contact(kalloc_,x)(){\
    if(head(x)==NULL) return NULL;\
    spin_lock(&lock(x));\
    void * ret=(void *) head(x);\
    if(ret){\
      head(x)=head(x)->nxt;\
      DEBUG(unsigned long j=0);\
      DEBUG(for(unsigned char * ptr=((unsigned char *)ret)+sizeof(free_list);j<x-sizeof(free_list);++ptr,++j) assert(*ptr==MAGIC_UNUSED));\
    }\
    spin_unlock(&lock(x));\
    DEBUG(memset(ret,MAGIC_USED,x));\
    return ret;\
  }

#define CAO_FREE(x)\
  static inline void contact(kfree_,x)(void * ptr){\
    free_list * hdr=ptr;\
    DEBUG(memset(ptr,MAGIC_UNUSED,x));\
    hdr->size=x;\
    spin_lock(&lock(x));\
    hdr->nxt=head(x)->nxt;\
    head(x)=hdr;\
    spin_unlock(&lock(x));\
  }
