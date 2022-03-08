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

#define STACK_SIZE (64*1024+32)
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

static void stack_switch_call(void * sp, void *entry, void * arg) {
  sp=(void *)( ((uintptr_t) sp & -16) );
//  DEBUG("%p %p %p\n",(void *)sp,entry,(void *)arg);
  asm volatile (
#if __x86_64__
    "movq %0, %%rsp; movq %2, %%rdi; call *%1"
      : : "b"((uintptr_t)sp), "d"(entry), "a"(arg) : "memory"
#else
    "movl %0, %%esp; movl %2, 8(%0); call *%1"
      : : "b"((uintptr_t)sp - 8), "d"(entry), "a"(arg) : "memory"
#endif
  );
  current->status=CO_DEAD;
  if(current->waiter) {
    assert(current->waiter->status==CO_WAITING);
    current->waiter->status=CO_RUNNING;
  }
  co_yield();
//  CAO_DEBUG("END REACH HERE!");
}

void co_wait(struct co *co) {
//  CAO_DEBUG(co->name);
  assert(co);assert(co->waiter==NULL);assert(current->status==CO_RUNNING);
  current->status=CO_WAITING;
  co->waiter=current;
  setjmp(current->context);
  if(current->status!=CO_DEAD) co_yield();
  assert(co->status==CO_DEAD&&current->status==CO_RUNNING);
  del_list(co);
  return;
}

void co_yield() {
  int val=setjmp(current->context);
  if(!val){
    for(current=current->nxt;current->status!=CO_RUNNING&&current->status!=CO_NEW;current=current->nxt);

    switch(current->status){
      case CO_NEW: 
        current->status=CO_RUNNING;
        stack_switch_call(current->stack+STACK_SIZE,current->func,current->arg);
      case CO_RUNNING: longjmp(current->context,1);
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