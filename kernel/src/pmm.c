#include "pmm.h"

uintptr_t kernel_max;

void * kernel_alloc(size_t size){
  void * ret=(void *)kernel_max;
  kernel_max+=size;
  return ret;
}

start_info * head_64,* head_256, * head_4096;
buddy * self;
int self_lock;

//init:
static inline start_info * init_start_info(){
  start_info * ret=(start_info *)kernel_alloc(sizeof(start_info));
  ret->head=NULL;ret->num_all=0;ret->lock=0;
  return ret;
}

static inline void init_mm(){
  kernel_max=HEAP_START;
  DEBUG(memset((void *)HEAP_START,MAGIC_UNUSED,HEAP_END-HEAP_START));

  head_64=init_start_info();head_256=init_start_info();head_4096=init_start_info();
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
    return head;
  }
  block->size=size;
  uintptr_t start=(uintptr_t)block;uintptr_t end=((uintptr_t)block)+Unit_size;
  for(uintptr_t ptr=ROUNDUP(start+sizeof(block_info),size);ptr<end;ptr+=size){
    free_list * now=(free_list *)ptr;
    now->nxt=head;head=now;
  }
  return head;
}

static inline void * kalloc_small(start_info * head,size_t size){
  free_list * ret=NULL;
  spin_lock(&head->lock);

  if(!head->head){
    spin_lock(&self_lock);
    int alloc_pages=head->num_all;
    if(alloc_pages==0) alloc_pages=1;
    for(int i=0;i<alloc_pages;++i){
      block_info * block=(block_info *)buddy_alloc(self,1);
      if(block) {
        head->head=init_pages(block,size,head->head);
        ++head->num_all;
      }
    }
    spin_unlock(&self_lock);
  }
  ret=head->head;
  if(ret) head->head=ret->nxt;
  printf("kalloc_small:%p %p\n",ret,head->head);
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
  size_t len=4096;
  start_info * head_size=head_4096;
  if(size<=256) len=256,head_size=head_256;
  if(size<=64) len=64,head_size=head_64;
  return kalloc_small(head_size,len);
}

//free
static inline void kfree_small(void * ptr,size_t len){
  free_list * now=(free_list *)ptr;
  DEBUG(memset((void *)(now+1),MAGIC_BIG,len);)
  start_info * head;
  switch (len){
    case 64:head=head_64;break;
    case 256:head=head_256;break;
    default:head=head_4096;break;
  }
  spin_lock(&head->lock);
  now->nxt=head->head;
  head->head=now;
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