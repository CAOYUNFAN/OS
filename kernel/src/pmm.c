#include "pmm.h"

uintptr_t kernel_max;

void * kernel_alloc(size_t size){
  void * ret=(void *)kernel_max;
  kernel_max+=size;
  return ret;
}

start_info * head_32[8],* head_128[8], * head_1024[8],* head_4096[8];
start_info_all * head_32_all, *head_128_all, *head_1024_all, *head_4096_all;
buddy * self;
int self_lock;

//init:
static inline start_info * init_start_info(){
  start_info * ret=(start_info *)kernel_alloc(sizeof(start_info));
  ret->head=NULL;ret->num_all=0;ret->lock=0;
  return ret;
}

static inline start_info_all * init_start_info_all(){
  start_info_all * ret=(start_info_all *)kernel_alloc(sizeof(start_info_all));
  ret->head=NULL;ret->num_all=16;ret->num_available=0;ret->lock=0;
  return ret;
}

static inline void init_mm(){
  kernel_max=HEAP_START;
  DEBUG(memset((void *)HEAP_START,MAGIC_UNUSED,HEAP_END-HEAP_START));

  for(uintptr_t i=0;i<8;++i){
    #define init_start(x) contact(head_,x)[i]=init_start_info();
    init_start(32)
    init_start(128)
    init_start(1024)
    init_start(4096)
  }
  head_32_all=init_start_info();
  head_128_all=init_start_info();
  head_1024_all=init_start_info();
  head_4096_all=init_start_info();
  self=buddy_init((HEAP_END-HEAP_OFFSET_START)/Unit_size);self_lock=0;
  return;
}

#ifndef TEST
static void pmm_init() {
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);
  init_mm();
  return;
}
#else
extern FILE * fd;
#define HEAP_SIZE (512*1024*1024)
static void pmm_init() {
  char *ptr  = malloc(HEAP_SIZE);
  heap.start = ptr;
  heap.end   = ptr + HEAP_SIZE;
  printf("Got %d MiB heap: [%p, %p)\n", HEAP_SIZE >> 20, heap.start, heap.end);
  fprintf(fd,"Got %d Byte heap: [%p, %p)\n", HEAP_SIZE , heap.start, heap.end);
  init_mm();
//  printf("Initialize memory Completed!\n");
  return;
}
#endif

//alloc:
static inline free_list * init_pages(block_info * block,size_t size,free_list * head){
  if(size==4096){
    free_list * now=(free_list *) block;
    now->nxt=head;
    return now;
  }
  block->size=size;
  uintptr_t start=(uintptr_t)block;uintptr_t end=((uintptr_t)block)+Unit_size;
  for(uintptr_t ptr=ROUNDUP(start+sizeof(block_info),size);ptr<end;ptr+=size){
    free_list * now=(free_list *)ptr;
    now->nxt=head;head=now;
  }
  return head;
}

static inline void get_pages(start_info_all * head,size_t size){
  spin_lock(&self_lock);
    int alloc_pages=head->num_all;
    for(int i=0;i<alloc_pages;++i){
      block_info * block=(block_info *)buddy_alloc(self,1);
      if(block) {
        head->head=init_pages(block,size,head->head);
        ++head->num_all;++head->num_available;
      }
    }
  spin_unlock(&self_lock);
}

static inline void * kalloc_small(start_info * head,size_t size,start_info_all * head_all){
  free_list * ret=NULL;
  spin_lock(&head->lock);

  if(!head->head){
    spin_lock(&head_all->lock);
    int nr_slub=16*(Unit_size==size?1:(Unit_size/size-1));
    if(head_all->num_available<nr_slub) get_pages(head_all,size);
    for(int i=0;i<nr_slub&&head_all->head;++i){
      free_list * now=head_all->head;
      head_all->head=now->nxt;
      now->nxt=head->head;
      head->head=now;
      head->num_all++;
    }
    spin_unlock(&head_all->lock);
  }
  ret=head->head;
  if(ret) head->head=ret->nxt,head->num_all--;
//  printf("kalloc_small:%p %p\n",ret,head->head);
  spin_unlock(&head->lock);
  #ifdef TEST
    if(ret){
      unsigned char * i=((unsigned char *)ret)+sizeof(free_list);
      for(uintptr_t j=sizeof(free_list);j<size;i++,j++) Assert(*i==MAGIC_BIG,"Unexpeted Magic %p=%x\n",i,*i);
      memset(ret,MAGIC_SMALL,size);
    }
  #endif
  return (void *)ret;
}

static void * kalloc(size_t size){
  if(size>MAX_alloc) return NULL;
  if(size>Unit_size){
    spin_lock(&self_lock);
    void * ret=buddy_alloc(self,ROUNDUP(size,Unit_size)/Unit_size);
    spin_unlock(&self_lock);
    return ret;
  }
  #define CONDITION(X) if(size<=X) return kalloc_small(contact(head_,X)[cpu_current()],X,contact(head_,contact(X,_all)));
  CONDITION(32)
  CONDITION(128)
  CONDITION(1024)
  CONDITION(4096)
  return NULL;
}

//free
static inline void kfree_small(void * ptr,size_t size){
  free_list * now=(free_list *)ptr;
  DEBUG(memset((void *)now,MAGIC_BIG,size);)
  Assert(LOWBIT((uintptr_t)ptr)>=len,"NOT aligned! %p,size=%d\n",ptr,size);
  start_info * head;
  start_info_all * head_all;
  #define CASE(X) case X: head=contact(head_,X)[cpu_current()];head_all=contact(head_,contact(X,_all));break;
  switch (size){
    CASE(32)
    CASE(128)
    CASE(1024)
    CASE(4096)
    default: head=head_4096[cpu_current()];head_all=head_4096_all;break;
  }
  int nr_slub=16*(Unit_size==size?1:(Unit_size/size-1));
  spin_lock(&head->lock);
  now->nxt=head->head;
  head->num_all++;
  head->head=now;
  if(head->num_all>=nr_slub*2){
    spin_lock(&head_all->lock);
    for(int i=nr_slub;i<head->num_all;i++){
      free_list * now=head->head;head->head=now->nxt;
      now->nxt=head_all->head;
      head_all->head=now;
      head_all->num_available++;
    }
    head->num_all=nr_slub;
    spin_unlock(&head_all->lock);
  }
  spin_unlock(&head->lock);
}

static void kfree(void * ptr){
  if(LOWBIT((uintptr_t)ptr)>Unit_size&&!is_block(self,(((uintptr_t)ptr)-HEAP_OFFSET_START)/Unit_size)){
    spin_lock(&self_lock);
    buddy_free(self,ptr);
    spin_unlock(&self_lock);
    return;
  }
  int len=Unit_size;
  if(LOWBIT((uintptr_t)ptr)<Unit_size){
    block_info * start=(block_info *)ROUNDDOWN(ptr,Unit_size);
    len=start->size;
  }
  kfree_small(ptr,len);
}

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};