#include <common.h>

#define MAX_malloc (16*1024*1024)
#define Unit_size (MAX_malloc<<3)
#define Unit_mask (-Unit_size)

#define MAGIC_UNUSED (0x7c)
#define MAGIC_UNLOCKED (0)
#define MAGIC_LOCKED (1)
#define MAGIC_MHD (0x19810114)

#define LOCK_ADDR(x) ((x)+Unit_size-sizeof(int))

typedef int spinlock_t;

typedef struct __free_list{
  uintptr_t size;
  struct __free_list *nxt;
}free_list;

typedef struct{
  uintptr_t size;
  uintptr_t magic;
}mem_head;

static uintptr_t begin,end,tot_num;

void spin_lock(spinlock_t *lk) {
  while (1) {
    intptr_t value = atomic_xchg(lk, MAGIC_LOCKED);
    if (value == MAGIC_UNLOCKED) {
      break;
    }
  }
}
void spin_unlock(spinlock_t *lk) {
  atomic_xchg(lk, MAGIC_UNLOCKED);
}

static inline uintptr_t check(free_list * now,size_t len){
  if(now->size<len+sizeof(mem_head)) return 0;
  return 0;
//  uintptr_t testptr=(uintptr_t) now+
}

static inline uintptr_t try_alloc(uintptr_t begin,uintptr_t end,size_t len){
  free_list * now=(free_list *)*((uintptr_t *)begin);
  uintptr_t l;
  while(now&&!(l=check(now,len))) now=now->nxt;
  if(!now) return 0;
  return 0;
}

static void *kalloc(size_t size) {
  if(size>MAX_malloc) return NULL;
  int vis_num=0;
  for(uintptr_t now=begin;vis_num<tot_num;now=(now+Unit_size==end?begin:now+Unit_size))
  if(atomic_xchg((int *)LOCK_ADDR(now),MAGIC_LOCKED)==MAGIC_UNLOCKED){
    ++vis_num;
    uintptr_t ret=try_alloc(now,LOCK_ADDR(now),size);
    spin_unlock((int *)LOCK_ADDR(now));
    if(ret) return (void *)ret;
  }

  return NULL;
}

static void add_list(uintptr_t pos,free_list * insert){
  free_list * head=(free_list *)(*(uintptr_t *)pos);
  if((uintptr_t)head>(uintptr_t)insert){
    insert->nxt=head;
    *(uintptr_t *)pos=(uintptr_t)insert;
  }else{
    while((uintptr_t)head+head->size<(uintptr_t)insert&&(head->nxt==NULL||(uintptr_t)head->nxt>(uintptr_t)insert)) head=head->nxt;
    insert->nxt=head->nxt;head->nxt=insert;
    if((uintptr_t)head+head->size==(uintptr_t)insert){
      head->size+=insert->size;head->nxt=insert->nxt;
      memset(insert,MAGIC_UNUSED,sizeof(free_list));
      insert=head;
    }
  }
  if((uintptr_t)insert+insert->size==(uintptr_t)insert->nxt){
    insert->size+=insert->nxt->size;
    free_list *temp=insert->nxt;
    insert->nxt=insert->nxt->nxt;
    memset(temp,MAGIC_UNUSED,sizeof(free_list));
  }
}

static void kfree(void *ptr) {
  spin_lock((int *)LOCK_ADDR((uintptr_t)ptr&Unit_mask));
  mem_head *mhd=(mem_head *)ptr-1;
  assert(mhd->magic==MAGIC_MHD);
  uintptr_t len=mhd->size;
  int j=0;
  for(unsigned char *i=(unsigned char *)ptr;j<len;++i,++j) *i=MAGIC_UNUSED;
  ((free_list *) mhd)->size=len+sizeof(mem_head);
  add_list((uintptr_t)ptr&Unit_mask,(free_list *)mhd);
  spin_unlock((int *)LOCK_ADDR((uintptr_t)ptr&Unit_mask));
  return;
}

static inline void init_mm(uintptr_t begin,uintptr_t end){
  tot_num=0;
  for(uintptr_t i=begin;i<end;i+=Unit_size,++tot_num){
    uintptr_t j=i+sizeof(uintptr_t);
    *((uintptr_t *)i)=j;
    ((free_list *)j)->size=Unit_size-sizeof(uintptr_t);
    ((free_list *)j)->nxt=NULL;
    for(j+=sizeof(free_list);j<i+Unit_size-sizeof(spinlock_t);j++) *((unsigned char *)j)=MAGIC_UNUSED;
    *((spinlock_t *)j)=MAGIC_UNLOCKED;
  }
  return;
}

static void pmm_init() {
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);

//  init_mm(begin=(((uintptr_t)heap.end-Unit_size+1)&Unit_mask)+Unit_size,end=(uintptr_t)heap.end&Unit_mask);
  return;
}

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};