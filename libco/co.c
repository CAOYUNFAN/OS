#include "co.h"
#include <stdlib.h>
#include <setjmp.h>
#include <assert.h>
#include <stdint.h>

#ifdef LOCAL_MACHINE
  #include <stdio.h>
  #define DEBUG(...) printf(__VA_ARGS__)
#else
  #define DEBUG()
#endif

#define CAO_DEBUG(name) DEBUG("[%3d:%10s]%s!\n",__LINE__,__FUNCTION__,name)

typedef enum{
  CO_NEW=1,
  CO_RUNNING,
  CO_WAITING,
  CO_DEAD,
}co_status;

#define STACK_SIZE (64*1024+16)
struct co {
  const char * name;
  void (*func) (void *);
  void * arg;

  co_status status;
  struct co * waiter;
  struct co * pre;
  struct co * nxt;
  jmp_buf context;
  uint8_t stack[STACK_SIZE] ;
};

struct co * st=NULL;

struct co * current=NULL;

void add_list(struct co * x){
  x->nxt=st->nxt;st->nxt=x;
  x->nxt->pre=x;x->pre=st;
  return;
}

void del_list(struct co * x){
  x->nxt->pre=x->pre;x->pre->nxt=x->nxt;
  free(x);
  return;
}

void * malloc__(size_t size){
  void * ret=malloc(size);
  while (ret==NULL) ret=malloc(size);
  return ret;
}

struct co *co_start(const char *name, void (*func)(void *), void *arg) {
  struct co *ret = malloc__(sizeof(struct co));
  ret->name=name;ret->func=func;ret->arg=arg;ret->waiter=NULL;
  add_list(ret);
  ret->status=CO_NEW;
  return ret;
}

void for_running(struct co *co){
//  CAO_DEBUG(co->name);
  current=co;
  longjmp(co->context,1);
  return;
}

static inline void stack_switch_call(void * sp, void *entry, uintptr_t arg) {
  sp=(void *)( ((uintptr_t) sp & -16) );
//  DEBUG("%p %p %p\n",(void *)sp,entry,(void *)arg);
  asm volatile (
#if __x86_64__
    "movq %0, %%rsp; movq %2, %%rdi; call *%1"
      : : "b"((uintptr_t)sp), "d"(entry), "a"(arg) : "memory"
#else
    "movl %0, %%esp; movl %2, 4(%0); call *%1"
      : : "b"((uintptr_t)sp - 8), "d"(entry), "a"(arg) : "memory"
#endif
  );
  CAO_DEBUG("END REACH HERE!");
}

void for_new(struct co * co){
//  CAO_DEBUG(co->name);
  current=co;co->status=CO_RUNNING;
//  DEBUG("%p\n",co->stack);
  stack_switch_call(co->stack+STACK_SIZE,co->func,(uintptr_t)co->arg);
  co->status=CO_DEAD;
  if(co->waiter) {
    assert(co->waiter->status==CO_WAITING);
    co->waiter->status=CO_RUNNING;
    for_running(co->waiter);
  }
  co_yield();
}

void co_wait(struct co *co) {
//  CAO_DEBUG(co->name);
  assert(co);assert(co->waiter==NULL);assert(current->status==CO_RUNNING);
  current->status=CO_WAITING;
  co->waiter=current;
  int tag=setjmp(current->context);
  while (!tag){
      switch (co->status) {
      case CO_NEW: for_new(co); break;
      case CO_RUNNING: for_running(co); break;
      case CO_WAITING: co_yield(); break;
      default: tag=1;break;
    }
  }
  assert(co->status==CO_DEAD&&current->status==CO_RUNNING);
  del_list(co);
  return;
}

void co_yield() {
  int val=setjmp(current->context);
  if(!val){
    current=current->nxt;
    while (current->status!=CO_NEW&&current->status!=CO_RUNNING) current=current->nxt;
    switch(current->status){
      case CO_NEW: for_new(current); break;
      case CO_RUNNING: for_running(current);break;
      default: assert(0);
    }
  }
  return;
}

void __attribute__((constructor)) init(){
  while(!( current=malloc__(sizeof(struct co)) ));
  st=current;current->pre=current->nxt=current;
  current->name="main";
  current->status=CO_RUNNING;  
}