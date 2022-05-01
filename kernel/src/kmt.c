#include <os.h>
#include "kmt-test.h"

static inline void lock_inside(int * addr,int * status){
    *status=ienabled();
    while (1) {
        if(atomic_xchg(addr,1)==0) {
            iset(false);
            return;
        }
    }
}
static inline void unlock_inside(int * addr,int status){
    atomic_xchg(addr,0);
    if(status) iset(true);
}

task_t * current_all[8]={};

static inline void init_list(list_head * list){
    list->head=NULL;list->lock=0;
    return;
}
list_head runnable;

static inline void add_list(list_head * list,task_t * task){
    int x=0;
    lock_inside(&list->lock,&x);
    if(!list->head){
        list->head=task;task->pre=task->nxt=task;
    }else{
        task->pre=list->head->pre;task->pre->nxt=task;
        task->nxt=list->head;list->head->pre=task;
    }
    unlock_inside(&list->lock,x);
    return;
}
static inline void del_core(list_head * list,task_t * task){
    Assert(list->head&&task,"SHOULD NOT BE NULL! head=%p,task=%p",list->head,task);
    if(list->head==task) {
        if(list->head==task->nxt) list->head=NULL;
        else list->head=task->nxt;
    }
    task->nxt->pre=task->pre;task->pre->nxt=task->nxt;
}
static inline void del_list(list_head * list,task_t * task){
    int x=0;
    lock_inside(&list->lock,&x);
    del_core(list,task);
    unlock_inside(&list->lock,x);
    return;
}
static inline task_t * del_list2(list_head * list){
    int x=0;
    lock_inside(&list->lock,&x);
    task_t * task=list->head;
    if(task) del_core(list,task);
    unlock_inside(&list->lock,x);
    return task;
}

static Context * kmt_context_save(Event ev,Context * ctx){
//    Log("save_context!");
    task_t * current=current_all[cpu_current()];
    Assert(current==NULL||current->status!=TASK_RUNABLE,"the status of %p SHOULD NOT be RUNNABLE!",current);
    if(current) current->ctx=ctx;
//    Log("%p %p",current,ctx);
    return NULL;
}

static Context * kmt_schedule(Event ev,Context * ctx){
    task_t * current=current_all[cpu_current()];
    if(ev.event == EVENT_SYSCALL || ev.event == EVENT_PAGEFAULT || ev.event == EVENT_ERROR) {
        Assert(current,"%p shou not be NULL!\n",current);
        return current->ctx;
    }
//    Log("Schedule!");
    if(ev.event==EVENT_IRQ_TIMER) return ctx;
    if(current&&current->status==TASK_RUNNING){
        current->status=TASK_RUNABLE;
        add_list(&runnable,current);
    }

    current=del_list2(&runnable);
    while (!current){
        Log("Current is NULL! CPU %d Waiting for the first Runnable program!",cpu_current());
        current=del_list2(&runnable);
    };

    Assert(current,"CPU%d:Current is NULL!",cpu_current());
    Assert(current->status==TASK_RUNABLE,"CPU%d: Unexpected status %d",current->status);
    current->status=TASK_RUNNING;

    current_all[cpu_current()]=current;
    Log("CPU%d:switch to task %s,%p",cpu_current(),current->name,current);
    return current->ctx;
}

static void kmt_init(){
    #  define INT_MIN	(-INT_MAX - 1)
    #  define INT_MAX	2147483647
    os->on_irq(INT_MIN,EVENT_NULL,kmt_context_save);
    os->on_irq(INT_MAX, EVENT_NULL, kmt_schedule);
    init_list(&runnable);

    #ifdef LOCAL
//    kmt->create(task_alloc(), "tty_reader", tty_reader, "tty1");
//    kmt->create(task_alloc(), "tty_reader", tty_reader, "tty2");
    #endif
}

static int kmt_create(task_t * task, const char * name, void (*entry)(void * arg),void * arg){
    assert(task);
    task->status=TASK_RUNABLE;
    task->stack=pmm->alloc(16*4096);
    Area temp;
    temp.start=task->stack;temp.end=(void *)((uintptr_t)task->stack+16*4096);
    task->ctx=kcontext(temp,entry,arg);
    #ifdef LOCAL
    task->name=name;
    #endif
//    Log("Task %s is added to %p",name,task);
    add_list(&runnable,task);
    return 0;
}

static void kmt_teardown(task_t * task){
    pmm->free(task->stack);
    Assert(task->status==TASK_RUNABLE||task->status==TASK_RUNNING,"task %p is blocked!\n",task);
    if(task->status==TASK_RUNABLE) del_list(&runnable,task);
    task->status=TASK_DEAD;
    free(task->stack);
}

static void kmt_spin_init(spinlock_t *lk, const char * name){
    init_list(&lk->head);lk->used=lk->lock=0;
    #ifdef LOCAL
    lk->name=name;
    #endif
    return;
}

static void kmt_spin_lock(spinlock_t *lk){
    int i=0;
    lock_inside(&lk->lock,&i);
    if(lk->used){
        task_t * current=current_all[cpu_current()];
        Assert(current->status==TASK_RUNNING,"Unexpected task status %d\n",current->status);
        current->status=TASK_WAITING;
        add_list(&lk->head,current);
        yield();
        Assert(current->status==TASK_RUNNING,"Unexpected task status %d\n",current->status);
    }else lk->used=1;
    unlock_inside(&lk->lock,i);
    return;
}

static void kmt_spin_unlock(spinlock_t * lk){
    int i=0;
    lock_inside(&lk->lock,&i);
    Assert(lk->used==1,"LOCK %p NOT LOCKED!",lk);
    task_t * next=del_list2(&lk->head);
    if(next==NULL) lk->used=0;
    else{
        Assert(next->status==TASK_WAITING,"Unexpected status %p,%d",next,next->status);
        next->status=TASK_RUNABLE;
        add_list(&runnable,next);
    }
    unlock_inside(&lk->lock,i);
    return;
}

static void kmt_sem_init(sem_t * sem,const char * name, int value){
    sem->num=value;sem->lock=0;init_list(&sem->head);
    #ifdef LOCAL
    sem->name=name;
    #endif
}

static void kmt_sem_wait(sem_t * sem){
    int i=0;
    lock_inside(&sem->lock,&i);
    sem->num--;
    if(sem->num<0){
        task_t * current=current_all[cpu_current()];
        Assert(current->status==TASK_RUNNING,"Unexpected task status %d\n",current->status);
        current->status=TASK_WAITING;
        Log("cpu%d,semlock-%s:task-%s is waiting!",cpu_current(),sem->name,current->name);
        Log("here!");
        add_list(&sem->head,current);assert(sem->head.lock==0);
        yield();
        Assert(current->status==TASK_RUNNING,"Unexpected task status %d\n",current->status);
    }
    unlock_inside(&sem->lock,i);
}

static void kmt_sem_signal(sem_t * sem){
    Log("here!");
    int i=0;
    lock_inside(&sem->lock,&i);
    task_t * next=del_list2(&sem->head);assert(sem->head.lock==0);
    sem->num++;Log("name=%s,left=%d",sem->name,sem->num);
    if(next){
        Assert(next->status==TASK_WAITING,"Unexpected status %p,%d",next,next->status);
        next->status=TASK_RUNABLE;
        add_list(&runnable,next);
    }
    unlock_inside(&sem->lock,i);
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
