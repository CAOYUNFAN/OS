#include <common.h>
#include "macros_for_pmm.h"

#define MAX_alloc (16*1024*1024)
#define Unit_size (4096)
#define HEAP_START ROUNDUP((uintptr_t)heap.start,Unit_size)
#define HEAP_END ROUNDDOWN((uintptr_t)heap.end,Unit_size)
#define HEAP_REAL_START (HEAP_START+Unit_size)
#define total_num ((HEAP_END-HEAP_START)/Unit_size)

static uintptr_t kernel_max; 
static void * kernel_alloc(size_t size){
    void * ret=(void *)kernel_max;
    kernel_max+=size;
    Assert(kernel_max<=HEAP_REAL_START,"TOO MUCH kernel use! %ld\n",size);
    return ret;
}

static free_list ** head;
typedef struct{
    free_list * header;
    int num;
}info_start;

static info_start * head_64;
static info_start * head_128;
static info_start * head_4096;

static inline void init_work(){
    int num=0;
    for(int i=4096;i<=(MAX_alloc<<1);i<<=1) ++num;
    head=kernel_alloc(sizeof(free_list *)*num);
    memset(head,0,sizeof(free_list *)*num);
    uintptr_t now=HEAP_END,len=4096,cen=0;
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
    head_64=kernel_alloc(sizeof(info_start));head_64->header=NULL;head_64->num=0;
    head_128=kernel_alloc(sizeof(info_start));head_128->header=NULL;head_128->num=0;
    head_4096=kernel_alloc(sizeof(info_start));head_4096->header=NULL;head_4096->num=0;
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
#define HEAP_SIZE (128*1024*1024+Unit_size)
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