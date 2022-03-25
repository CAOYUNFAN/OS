#include <common.h>
#include "macros_for_pmm.h"
#include "buddy.h"

static uintptr_t kernel_max; 
void * kernel_alloc(size_t size){
  void * ret=(void *)kernel_max;
  kernel_max+=size;
  Assert(kernel_max<=HEAP_REAL_START,"TOO MUCH kernel use! %ld\n",size);
  return ret;
}

static free_list ** head;
typedef struct{
    block_start * header;
    int num,lock;
}info_start;

static info_start * head_64;
static info_start * head_128;
static info_start * head_4096;

typedef struct {
  int size,lock;
  free_list * start;
  block_start * nxt;
}block_start;

static inline void init_block(uintptr_t ptr,size_t size){
  Assert(LOWBIT(ptr)>=Unit_size,"Do not alloc a page %p!\n",(void *)ptr);
  Assert(size==64||size==256,"Unexpected size %ld!\n",size);
  block_start * start=(block_start *)ptr;
  start->size=size;start->start=NULL;start->lock=0;start->nxt=NULL;
  for(uintptr_t i=ROUNDUP(ptr+sizeof(block_start),size);i<ptr+Unit_size;i+=size){
    free_list * temp=(free_list *)i;
    temp->size=size;temp->nxt=start->start;start->start=temp;
  }
  return;
}

static inline void get_pages(info_start * head,size_t len,block_start **start){
}

static inline void insert(block_start * block,free_list * insert){
  Assert(LOWBIT((uintptr_t)block)>=Unit_size,"Do not input a page %p!\n",block);
  int size=block->size;
  Assert(LOWBIT((uintptr_t)insert)>=size,"Insert is not alligned %p\n",insert);
  Assert((uintptr_t)insert>=(uintptr_t)(block+1)&&((uintptr_t)insert)+size<=((uintptr_t)block)+Unit_size,"Insert %p is not in Block %p",insert,block);
  DEBUG(memset(insert,MAGIC_UNUSED,size);)
  insert->size=size;
  spin_lock(&block->lock);
  insert->nxt=block->start;block->start=insert;
  spin_unlock(&block->lock);
}

static inline void * get(block_start * block){
  Assert(LOWBIT((uintptr_t)block)>=Unit_size,"Do not input a page %p!\n",block);
  Assert(block->lock==MAGIC_LOCKED,"IT IS NOT LOCKED!!! %p\n",block);
  int size=block->size;
  Assert(size==64||size==256,"Block %p errors,len=%ld!\n",block,len);
  void * ret=(void *)block->start;
  if(ret) block->start=block->start->nxt;
  DEBUG(memset(ret,size,MAGIC_USED));
}

static inline void * slab_alloc(info_start * head,size_t len){
  void * ret=NULL;
  block_start * start;
  spin_lock(&head->lock);
  start=head->header;
  spin_unlock(&head->lock);
  for(;start!=NULL;start=start->nxt)
  if(start->start&&atomic_xchg(&start->lock,MAGIC_LOCKED)){
    ret=get(start);
    spin_unlock(&start->lock);
    if(!ret) continue;
    return ret;
  }
  get_pages(head,len,&start);
  for(;start!=NULL;start=start->nxt)
  if(start->start&&atomic_xchg(&start->lock,MAGIC_LOCKED)){
    ret=get(start);
    spin_unlock(&start->lock);
    return ret;
  }
  return ret;
}

static inline void init_work(){
    int num=0;
    for(int i=Unit_size;i<=(MAX_alloc<<1);i<<=1) ++num;
    head=kernel_alloc(sizeof(free_list *)*num);
    memset(head,0,sizeof(free_list *)*num);
    uintptr_t now=HEAP_END,len=Unit_size,cen=0;
    for(;(now-len)>=HEAP_START&&LOWBIT(now)<(MAX_alloc<<1);len<<=1,++cen)
    if(LOWBIT(now)==len){
        free_list* temp=(free_list *)(now-len);
        temp->size=len;temp->nxt=head[cen];head[cen]=temp;
        now-=len;
    }
    for(;now-len>=HEAP_REAL_START;now-=len){
        Assert(len==(MAX_alloc<<1)&&cen==(num-1),"%s\n","Initialization Errors!");
        free_list * temp=(free_list *)(now-len);
        temp->size=len;temp->nxt=head[cen];head[cen]=temp;
    }
    for(;now>HEAP_REAL_START;len>>=1,--cen)
    if(now-len>=HEAP_REAL_START){
        free_list * temp=(free_list *)(now-len);
        temp->size=len;temp->nxt=head[cen];head[cen]=temp;
        now-=len;
    }
}

static inline void init_mm(){
  kernel_max=HEAP_START;
  DEBUG(memset(HEAP_START,MAGIC_UNUSED,HEAP_END-HEAP_START));
  init_work();
  head_64=kernel_alloc(sizeof(info_start));head_64->header=NULL;head_64->num=0;head_64->lock=0;
  head_128=kernel_alloc(sizeof(info_start));head_128->header=NULL;head_128->num=0;head_128->lock=0;
  head_4096=kernel_alloc(sizeof(info_start));head_4096->header=NULL;head_4096->num=0;head_4096->lock=0;
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
#define HEAP_SIZE (512*1024*1024+Unit_size)
static void pmm_init() {
  char *ptr  = malloc(HEAP_SIZE);
  heap.start = ptr;
  heap.end   = ptr + HEAP_SIZE;
  printf("Got %d MiB heap: [%p, %p)\n", HEAP_SIZE >> 20, heap.start, heap.end);
  printf("free_list size=%ld,mem_tag size=%d\n",sizeof(free_list),sizeof(mem_tag));
  fprintf(fd,"Got %d Byte heap: [%p, %p)\n", HEAP_SIZE , heap.start, heap.end);
  init_mm();
  printf("num:%lx\n",total_num);
//  printf("Initialize memory Completed!\n");
  return;
}
#endif


static void * kalloc(size_t size){
    if(size>MAX_alloc) return NULL;
    return NULL;
}

static void kfree(void * ptr){

}

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};