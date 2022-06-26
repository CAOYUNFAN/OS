#include <os.h>
#include <syscall.h>

#include "initcode.inc"
#include "uproc.h"

extern task_t * current_all[8];
extern task_t * task_all_pid[32768];

static inline void lock_inside_ker(int * addr){
    while (1) {
        if(atomic_xchg(addr,1)==0) {
            iset(false);
            return;
        }
    }
}
static inline void unlock_inside_ker(int * addr){
    atomic_xchg(addr,0);
}
static inline void lock_inside(int * addr,int * status){
    *status=ienabled();
    lock_inside_ker(addr);
}
static inline void unlock_inside(int * addr,int status){
    unlock_inside_ker(addr);
    if(status) iset(true);
}

void uproc_clear_space(utaskk * ut){
    ut->as;
    return;
}

static void uproc_init(){
    vme_init(pmm->alloc, pmm->free);
    return;
}

static inline uint64_t get_time_us(){
    return io_read(AM_TIMER_UPTIME).us;
}

static int uproc_kputc(task_t * task,char ch){
    putch(ch); // safe for qemu even if not lock-protected
    return 0;
}

static int uproc_fork(task_t *task){
    return -1;
}

static int uproc_wait(task_t * task,int * status){
    Assert(task==current_all[cpu_current()],"unexpected task %s",task->name);
    Assert(task->status==TASK_RUNNING&&task->lock,"Unexpected current %s,status %d, lock %d",task->name,task->status,task->lock);
    if(!task->ch)return -1;
    while (!release) {
        task_t ** pre=&task->ch;
        for(task_t * temp=task->ch;temp;pre=&temp->bro,temp=temp->bro) if(temp->status==TASK_DEAD){
            if(status) *status=temp->ret;
            *pre=temp->bro;
            return 0;
        }
        yield();
    }
    return 0;
}

static int uproc_exit(task_t *task, int status){
    Assert(task==current_all[cpu_current()],"unexpected task %s",task->name);
    Assert(task->status==TASK_RUNNING&&task->lock,"Unexpected current %s,status %d, lock %d",task->name,task->status,task->lock);
    task->ret=status;
    kmt->teardown(task);
    yield();
    Assert(0,"SHOULD NOT REACH HERE! %s",task->name);
    return 0;
}

static int uproc_kill(task_t *task, int pid){
    if(pid<0) pid =-pid;
    Assert(task==current_all[cpu_current()],"unexpected task %s",task->name);
    Assert(task->status==TASK_RUNNING&&task->lock,"Unexpected current %s,status %d, lock %d",task->name,task->status,task->lock);
    Assert(task_all_pid[pid],"pid %d should not be NULL!",pid);
    kmt->teardown(task_all_pid[pid]);
    return 0;
}

static void * uproc_mmap(task_t *task, void *addr, int length, int prot, int flags){
    return NULL;
}

static int uproc_getpid(task_t *task){
    return task->pid;
}

static int uproc_sleep(task_t *task, int seconds){
    Assert(task->lock==1,"Name %s do not hlod the lock!",task->name);
    uint64_t awake=get_time_us()+(uint64_t)seconds*1000000uLL;
    while(get_time_us()<awake) yield();
    return 0;
}
  
static int64_t uproc_uptime(task_t *task){
    return (int64_t)(get_time_us()/1000uLL);
}

MODULE_DEF(uproc) = {
    UPROC_NAME(init)
    UPROC_NAME(kputc)
    UPROC_NAME(fork)
    UPROC_NAME(wait)
    UPROC_NAME(exit)
    UPROC_NAME(kill)
    UPROC_NAME(mmap)
    UPROC_NAME(getpid)
    UPROC_NAME(uptime)
};

#define NAME_RELATION(name,...) \
    case SYS_ ## name : ctx->GPRx = (uintptr_t) uproc -> name (task , ## __VA_ARGS__); break;

Context * syscall(task_t * task,Context * ctx){
    switch (ctx->GPRx) {
        NAME_RELATION(kputc,ctx->GPR1)
        NAME_RELATION(fork)
        NAME_RELATION(wait,c->GPR1)
        NAME_RELATION(exit,c->GPR1)
        NAME_RELATION(kill,c->GPR1)
        NAME_RELATION(mmap,c->GPR1,c->GPR2,c->GPR3,c->GPR4)
        NAME_RELATION(getpid)
        NAME_RELATION(uptime)   
        default: ctx->GPRx=-1; break;
    }
    return ctx;
}
