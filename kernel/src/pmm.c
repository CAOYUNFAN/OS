#include <common.h>
#include "macros_for_pmm.h"

#define MAX_malloc (16*1024*1024)
#define Unit_size (MAX_malloc)
#define Unit_mask (-Unit_size)

#define HEAP_START ROUNDUP((uintptr_t)heap.start,Unit_size)
#define HEAP_END ROUNDDOWN((uintptr_t)heap.end,Unit_size)
#define total_num ((HEAP_END-HEAP_START)/Unit_size)

#define LOWBIT(x) ((x)&((x)^((x)-1)))

#define MAGIC_MTG (0x13131313)

#define MTG_addr(pos,len) ((mem_tag *)((uintptr_t)pos+len-sizeof(mem_tag)))

typedef struct __free_list{
  uintptr_t size;
  struct __free_list *nxt;
}free_list;

typedef struct{
  uintptr_t size;
  uintptr_t magic;
}mem_tag;

static uintptr_t sbrk_now=0;

static inline void * kernel_alloc(size_t len){
  sbrk_now-=len;
  return (void *)sbrk_now;
}

CAO_FIXED_INIT(64,HEAP_START,(total_num+1)*3/8*Unit_size/2)
CAO_ALLOC(64)
CAO_FREE(64)

CAO_FIXED_INIT(256,heap_64_end,(total_num+1)/8*(Unit_size/2))
CAO_ALLOC(256)
CAO_FREE(256)

CAO_FIXED_INIT(4096,heap_256_end,total_num/4*Unit_size)
CAO_ALLOC(4096)
CAO_FREE(4096)

static uintptr_t heap_rest_start,heap_rest_end;
static free_list ** start_of_rest;
static int lock_rest;
void init_rest(){
  heap_rest_start=heap_4096_end;heap_rest_end=HEAP_END-Unit_size;
  uintptr_t num=0;
  for(uintptr_t i=4096;i<=(MAX_malloc);i<<=1) num++;
  num++;
  start_of_rest=kernel_alloc(sizeof(free_list *)*num);
  uintptr_t j=0;
  for(uintptr_t i=4096;i<MAX_malloc;i<<=1,++j) start_of_rest[j]=NULL;
  start_of_rest[j]=NULL;
  if(heap_rest_end%(MAX_malloc<<1)!=0){
    start_of_rest[j]=(free_list *)(heap_rest_end-MAX_malloc);
    start_of_rest[j]->size=MAX_malloc;
    start_of_rest[j]->nxt=NULL;
  }
  if(heap_rest_start%(MAX_malloc<<1)!=0){
    free_list * temp=(free_list *)heap_rest_start;
    temp->size=MAX_malloc;temp->nxt=start_of_rest[j];
    start_of_rest[j]=temp;
    printf("BEGIN:%p->%p->%p\n",start_of_rest[j],start_of_rest[j]->nxt,start_of_rest[j]->nxt->nxt);
  }
  ++j;
  if(ROUNDUP(heap_rest_start,MAX_malloc<<1)==ROUNDDOWN(heap_rest_end,MAX_malloc<<1)) start_of_rest[j]=NULL;
  else{
    start_of_rest[j]=(free_list *)ROUNDUP(heap_rest_start,MAX_malloc<<1);
    start_of_rest[j]->size=MAX_malloc<<1;
    for(uintptr_t ptr=ROUNDUP(heap_rest_start,MAX_malloc<<1)+(MAX_malloc<<1);ptr<ROUNDDOWN(heap_rest_end,MAX_malloc<<1);ptr+=MAX_malloc<<1){
      ((free_list *)ptr)->size=MAX_malloc<<1;
      ((free_list *)(ptr-(MAX_malloc<<1)))->nxt=(free_list *)ptr;
    }
    ((free_list *)(ROUNDDOWN(heap_rest_end,MAX_malloc<<1)-(MAX_malloc<<1)))->nxt=NULL;
  }  
}
static inline void insert(free_list * insert,free_list ** head){
  Assert(*head==NULL||insert->size==(*head)->size,"INSERT:%p->%ld,%p->%ld\n",insert,insert->size,*head,(*head)->size);
  if(*head==NULL||(uintptr_t)*head>(uintptr_t)insert){
    insert->nxt=*head;
    *head=insert;
    return;
  }
  for(free_list * now=*head;now!=NULL;now=now->nxt)
  if((uintptr_t)now<(uintptr_t)insert&&(now->nxt==NULL||(uintptr_t)now->nxt>(uintptr_t)insert)){
    insert->nxt=now->nxt;
    now->nxt=insert;
    return;
  }
}
static inline void * kalloc_rest(size_t size){
  size+=sizeof(mem_tag);
  free_list * ret=NULL;
  spin_lock(&lock_rest);
  for(uintptr_t i=4096,j=0;i<=MAX_malloc<<1;i<<=1,++j){
    if(i<size||!start_of_rest[j]) continue;
    ret=start_of_rest[j];
    start_of_rest[j]=start_of_rest[j]->nxt;
    #ifdef TEST
    unsigned long jj=0;
    if(!(ret->nxt==NULL||ret->nxt->size==i)) printf("%p:%ld-%p:%ld\n",ret,i,ret->nxt,ret->nxt->size);
    assert(ret->nxt==NULL||ret->nxt->size==i);
    for(unsigned char * ptr=((unsigned char *)ret)+sizeof(free_list);jj<i-sizeof(free_list);++ptr,++jj) {
      if(*ptr!=MAGIC_UNUSED) {
        printf("%p-%lx,size=%ld\n",ptr,jj,size);
        printf("%c\n",*ptr);
        printf("%p\n",((free_list *)ret)->nxt);
      }
      assert(*ptr==MAGIC_UNUSED);
    }
    #endif
    for(;(i>>1)>=size;i>>=1){
      ret->size>>=1;
      free_list * divide=(free_list *)((uintptr_t)ret+ret->size);
      divide->size=(i>>1);
      insert(divide,&start_of_rest[--j]);
    }
    DEBUG(memset(ret,MAGIC_USED,i);)
    MTG_addr(ret,i)->size=i;MTG_addr(ret,i)->magic=MAGIC_MTG;
    break;
  }
  spin_unlock(&lock_rest);
  return (void *)ret;
}
static inline free_list * update(free_list ** head){
  if(*head==NULL) return NULL;
  uintptr_t len=(*head)->size;
  if(len==MAX_malloc) return NULL;
  if(LOWBIT((uintptr_t)*head)>len&&(uintptr_t)*head+len==(uintptr_t)((*head)->nxt)){
    free_list * ret=*head;
    *head=(*head)->nxt->nxt;
    ret->size=(len<<1);
    #ifdef TEST
    memset((void *)ret->nxt,MAGIC_UNUSED,sizeof(free_list));
    int jj=0;
    for(unsigned char *x=((unsigned char *)ret+sizeof(free_list));jj<(len<<1)-sizeof(free_list);++jj,++x) {
      if(*x!=MAGIC_UNUSED) printf("%p-%lx\n",x,jj+sizeof(free_list));
      assert(*x==MAGIC_UNUSED);
    }
    #endif
    return ret;
  }
  for(free_list * now=*head;now->nxt!=NULL;now=now->nxt){
    free_list * next=now->nxt;
    if(LOWBIT((uintptr_t)next)>len&&(uintptr_t)next+len==(uintptr_t)(next->nxt)){
      now->nxt=next->nxt->nxt;
      next->size=len<<1;
      DEBUG(memset(next->nxt,MAGIC_UNUSED,sizeof(free_list));)
      return next;
    }
  }
  return NULL;
}
static inline void kfree_rest(void * ptr){
  uintptr_t len=LOWBIT((uintptr_t)ptr);
  for(;len;len>>=1){
    if((uintptr_t)ptr+len<=heap_rest_end&&(MTG_addr(ptr,len)->magic|MAGIC_MTG)==MAGIC_MTG&&MTG_addr(ptr,len)->size==len) break;
  }
  DEBUG(memset((void *)ptr,MAGIC_UNUSED,len);)
  int pos=0;
  for(uintptr_t i=4096;i<len;i<<=1) ++pos;
  ((free_list *)ptr)->size=len;
  spin_lock(&lock_rest);
  insert(ptr,&start_of_rest[pos]);
  while((ptr=update(&start_of_rest[pos]))!=NULL) insert(ptr,&start_of_rest[++pos]);
  spin_unlock(&lock_rest);
}

void init_mm(){
  sbrk_now=HEAP_END;
  DEBUG(memset((void *)HEAP_START,MAGIC_UNUSED,HEAP_END-HEAP_START);)
  init_64();init_256();init_4096();init_rest();
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
#define HEAP_SIZE 128*1024*1024+Unit_size
static void pmm_init() {
  char *ptr  = malloc(HEAP_SIZE);
  heap.start = ptr;
  heap.end   = ptr + HEAP_SIZE;
  printf("Got %d MiB heap: [%p, %p)\n", HEAP_SIZE >> 20, heap.start, heap.end);
  fprintf(fd,"Got %d MiB heap: [%p, %p)\n", HEAP_SIZE >> 20, heap.start, heap.end);
  init_mm();
  printf("num:%lx\n",total_num);
  printf("128:[%lx,%lx) 4096:[%lx,%lx) rest: [%lx,%lx)\n",heap_128_start,heap_128_end,heap_4096_start,heap_4096_end,heap_rest_start,heap_rest_end);
//  printf("Initialize memory Completed!\n");
  return;
}
#endif

static void * kalloc(size_t size){
  if(size>MAX_malloc) return NULL;
  if(size<=64) return kalloc_64();
  if(size<=256) return kalloc_256();
  if(size<=4096) return kalloc_4096();
  return kalloc_rest(size);
}

static void kfree(void * ptr){
  if((uintptr_t)ptr>=heap_64_start&&(uintptr_t)ptr<heap_64_end) kfree_128(ptr);
  if((uintptr_t)ptr>=heap_256_start&&(uintptr_t)ptr<heap_256_end) kfree_256(ptr);
  if((uintptr_t)ptr>=heap_4096_start&&(uintptr_t)ptr<heap_4096_end) kfree_4096(ptr);
  if((uintptr_t)ptr>=heap_rest_start&&(uintptr_t)ptr<heap_rest_end) kfree_rest(ptr);
}

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};
