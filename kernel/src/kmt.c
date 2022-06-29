#include <os.h>
#include "kmt-test.h"
#include "kmt.h"

task_t * current_all[8]={};
static task_t * previous_all[8]={};

static inline void task_queue_init(task_queue * q){
    q->head=q->tail=NULL;
    q->lock=0;
    return;
}
static task_queue runnable;

static Context * kmt_context_save(Event ev,Context * ctx){
    Log("save_context!");
    task_t * current=current_all[cpu_current()];
//    Assert(current==NULL||current->status!=TASK_RUNABLE,"the status %d of %s SHOULD NOT be RUNNABLE!",current->status,current->name);
    if(current&&current->status!=TASK_DEAD) current->ctx[current->nc++]=ctx;
    Assert(!current||current->nc==1||current->nc==2,"%s traped too much times!\n",current->name);
    if(current) Log("save_context! %d",current->nc);else Log("save_context! NULL");
//    Log("%p %p",current,ctx);
    task_t * previous=previous_all[cpu_current()];
    if(previous){
        Assert(previous->lock==1,"previos %s should be locked!",previous->name);
        Assert(previous!=current,"previous %s is the same as current!",previous->name);
        unlock_inside_ker(&previous->lock);
        previous_all[cpu_current()]=NULL;
    } 
    return NULL;
}

static inline void real_free(task_t * task){
    pmm->free(task->stack);
    extern void uproc_clear_space(utaskk * ut);
    uproc_clear_space(&task->utask);
}

static Context * kmt_schedule(Event ev,Context * ctx){
    task_t * current=current_all[cpu_current()];
    if(ev.event == EVENT_SYSCALL || ev.event == EVENT_PAGEFAULT ) {
        Assert(current,"%p shou not be NULL!\n",current);
        return NULL;
    }
    Log("Schedule!");
/*#ifdef LOCAL
    if(ev.event==EVENT_IRQ_TIMER) return ctx;
#endif
*/
    if(current&&current->status==TASK_RUNNING){
        current->status=TASK_RUNABLE;
        task_queue_push(&runnable,current);
    }
    if(current) {
        Assert(previous_all[cpu_current()]==NULL,"previous %s has not been emptied!",previous_all[cpu_current()]->name);
        Assert(current->lock==1,"Unexpected lock status %d with name %s!",current->lock,current->name);
        previous_all[cpu_current()]=current;
    }

    task_t * pre=current;
    current=task_queue_pop(&runnable);
    while (!current||current->status!=TASK_RUNABLE||(pre!=current&&atomic_xchg(&current->lock,1))){
        //if(!current) Log("Current is NULL! CPU %d Waiting for the first Runnable program!",cpu_current());
        Assert(current==NULL||current->status==TASK_RUNABLE||current->status==TASK_DEAD,"%s unexpected status %d",current->name,current->status);
        if(current->status==TASK_DEAD){
            if(current!=pre) real_free(current);
            else task_queue_push(&runnable,current);
        }
        if(current&&current->status==TASK_RUNABLE&&pre!=current) task_queue_push(&runnable,current);
        current=task_queue_pop(&runnable);
    };
    Assert(current,"CPU%d:Current is NULL!",cpu_current());
    Assert(current->status==TASK_RUNABLE,"CPU%d: Unexpected status %d",current->status);
    if(current==pre) previous_all[cpu_current()]=NULL;
    current->status=TASK_RUNNING;

    current_all[cpu_current()]=current;
//    Log("switch to task %s,%p",current->name,current);
    Assert(current->nc==1||current->nc==2,"%s traped too much times!",current->name);
    return current->ctx[--current->nc];
}

static void kmt_teardown(task_t * task){
    Assert(task->status==TASK_RUNABLE||task->status==TASK_RUNNING,"task %p is blocked!\n",task);
    Assert(task_all_pid[task->pid]==task,"Invalid pid %s!",task->name);
    Log("task %s is ended",task->name);
    task_all_pid[task->pid]=NULL;
    task->status=TASK_DEAD;
    task->ch=NULL;
    pid_free(task->pid);
}

static Context * kmt_pagefault(Event ev,Context * ctx){
    Assert(current_all[cpu_current()]->nc==1,"%s multitrap of pagefault!",current_all[cpu_current()]->name);
    Log("%p %d",ev.ref,ev.cause);
    extern void pagefault_handler(void * va,int prot,task_t * task);
    pagefault_handler(get_vaddr(ev.ref),ev.cause,current_all[cpu_current()]);
//    printf("pf:%p by %p \n",ev.ref,ctx->rip);
    return ctx;
}

static Context * kmt_syscall(Event ev,Context * ctx){
    Assert(current_all[cpu_current()]->nc==1,"%s multitrap of pagefault!",current_all[cpu_current()]->name);
    task_t * current=current_all[cpu_current()];
    extern Context * syscall(task_t * task,Context * ctx);
    ctx=syscall(current,ctx);
    return ctx;
}

static Context * kmt_error(Event ev,Context * ctx){
    Assert(0,"%s error happens!",current_all[cpu_current()]->name);
    kmt_teardown(current_all[cpu_current()]);
    return NULL;
}

static void kmt_init(){
    #  define INT_MIN	(-INT_MAX - 1)
    #  define INT_MAX	2147483647
    os->on_irq(INT_MIN,EVENT_NULL,kmt_context_save);
    os->on_irq(INT_MAX, EVENT_NULL, kmt_schedule);
    os->on_irq(INT_MIN+10,EVENT_PAGEFAULT,kmt_pagefault);
    os->on_irq(INT_MIN+15,EVENT_SYSCALL,kmt_syscall);
    os->on_irq(INT_MIN+15,EVENT_ERROR,kmt_error);
    task_queue_init(&runnable);

    #ifdef LOCAL
//    kmt->create(task_alloc(), "tty_reader1", tty_reader, "tty1");
//    kmt->create(task_alloc(), "tty_reader2", tty_reader, "tty2");
    #endif
}

int create_all(task_t * task, const char * name, void (*entry)(void * arg), void * arg, Context * ctx){
    assert(task);assert((entry==NULL && ctx!=NULL)||(entry!=NULL && ctx ==NULL));
    task->status=TASK_RUNABLE;
    task->lock=0;
    task->ctx[0]=NULL;
    if(entry) {
        task->stack=pmm->alloc(16*4096);
        Area temp;
        temp.start=task->stack;temp.end=(void *)((uintptr_t)task->stack+16*4096);
        task->ctx[0]=kcontext(temp,entry,arg);task->nc=1;
        memset(&task->utask,0,sizeof(utaskk));
    }else{
        task->ctx[0]=ctx;task->nc=1;
    }
    #ifdef LOCAL
    task->name=name;
    #endif
    task->pid=new_pid();
    task_t * current=current_all[cpu_current()];Assert(current->lock,"CURRENT %s IS NOT LOCKED!",current->name);
    task->ch=NULL;
    task->bro=current->ch;current->ch=task;
    task->ret=0;
    task_all_pid[task->pid]=task;
//    Log("Task %s is added to %p",name,task);
    task_queue_push(&runnable,task);
    return task->pid;
}

static int kmt_create(task_t * task, const char * name, void (*entry)(void * arg),void * arg){
    create_all(task,name,entry,arg,NULL);
    return 0;
}

static void kmt_spin_init(spinlock_t *lk, const char * name){//Log("building %s",name);
    task_queue_init(&lk->head);lk->used=lk->lock=lk->status=0;
    #ifdef LOCAL
    lk->name=name;
    #endif
    return;
}

static void kmt_sleep(task_queue * q,int * lock_addr,int nxtstatus){
    task_t * current=current_all[cpu_current()];
    Assert(current&&current->lock&&current->status==TASK_RUNNING,"current %s is not running correctly!",current->name);
    Assert(*lock_addr==1,"%s is not keeping lock!",current->name);
    current->status=TASK_WAITING;
    task_queue_push(q,current);
//    Log("                                          Lock task name=%s",current->name);
    unlock_inside(lock_addr,nxtstatus);
    yield();
    Assert(current->status==TASK_RUNNING,"Unexpected task status %s with status %d",current->name,current->status);
    return;
}

static int kmt_wakeup(task_queue * q){
    task_t * nxt=task_queue_pop(q);
    if(!nxt) return 1;
    Assert(nxt->status==TASK_WAITING,"Unexpected task status %s with status %d",nxt->name,nxt->status);
    nxt->status=TASK_RUNABLE;
//    Log("                                          Free task name=%s",nxt->name);
    task_queue_push(&runnable,nxt);
    return 0;
}

static void kmt_spin_lock(spinlock_t *lk){
    if(current_all[cpu_current()]->status==TASK_DEAD){
        yield();
        Assert(0,"%s Unexpected reach!",current_all[cpu_current()]->name);
    }
    int i=0;
    lock_inside(&lk->lock,&i);
    
    if(lk->used){
//        Log("                                          LK_LOCK:spinlock name %s:",lk->name);
        kmt_sleep(&lk->head,&lk->lock,0);
        int tt=0;
        lock_inside(&lk->lock,&tt);
        Assert(tt==0,"%d should be zero!",tt);
        lk->status=i;
        unlock_inside(&lk->lock,0);
    }else{
        lk->used=1;
        unlock_inside(&lk->lock,0);
        lk->status=i;
    } 
    return;
}

static void kmt_spin_unlock(spinlock_t * lk){
    int i=0;
    lock_inside(&lk->lock,&i);
    Assert(i==0,"TASK %s: Interrupt is not closed!",current_all[cpu_current()]->name);
    Assert(lk->used==1,"LOCK %p NOT LOCKED!",lk);
    if(kmt_wakeup(&lk->head)) lk->used=0;
    else{
//        Log("                                          LK_FREE:spinlock name %s:",lk->name);
    } 
    i=lk->status;
    unlock_inside(&lk->lock,i);
    if(current_all[cpu_current()]->status==TASK_DEAD){
        yield();
        Assert(0,"%s Unexpected reach!",current_all[cpu_current()]->name);
    }
    return;
}

static void kmt_sem_init(sem_t * sem,const char * name, int value){//Log("building %s",name);
    sem->num=value;sem->lock=0;task_queue_init(&sem->head);
    #ifdef LOCAL
    sem->name=name;
    #endif
}

static void kmt_sem_wait(sem_t * sem){
    if(current_all[cpu_current()]->status==TASK_DEAD){
        yield();
        Assert(0,"%s Unexpected reach!",current_all[cpu_current()]->name);
    }
    int i=0;
    lock_inside(&sem->lock,&i);
    sem->num--;
//    Log("Wait name %s,num=%d",sem->name,sem->num);

    if(sem->num<0) {
//        Log("                                          SEM_LOCK:semlock name %s:",sem->name);
        kmt_sleep(&sem->head,&sem->lock,i);
    }
    else unlock_inside(&sem->lock,i);
}

static void kmt_sem_signal(sem_t * sem){
    int i=0;
    lock_inside(&sem->lock,&i);
    sem->num++;
//    Log("Sign name %s,num=%d",sem->name,sem->num);
    if(!kmt_wakeup(&sem->head)) {
//        Log("                                          SEM_FREE:semlock name %s",sem->name);
        Assert(sem->num<=0,"%d SHOULD BELOW ZERO!",sem->num);
    }else Assert(sem->num>0,"%d SHOULD ABOVE ZERO!",sem->num);
    unlock_inside(&sem->lock,i);
    if(current_all[cpu_current()]->status==TASK_DEAD){
        yield();
        Assert(0,"%s Unexpected reach!",current_all[cpu_current()]->name);
    }
} 

MODULE_DEF(kmt) = {
    . init=kmt_init,
    .create=kmt_create,
    .teardown=kmt_teardown,
    .spin_init=kmt_spin_init,
    .spin_lock=kmt_spin_lock,
    .spin_unlock=kmt_spin_unlock,
    .sem_init=kmt_sem_init,
    .sem_wait=kmt_sem_wait,
    .sem_signal=kmt_sem_signal
};
