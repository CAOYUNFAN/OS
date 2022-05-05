#include <os.h>
#include "kmt-test.h"

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

static task_t * current_all[8]={};

static inline void task_queue_init(task_queue * q){
    q->head=q->tail=NULL;
    q->lock=0;
    return;
}
static task_queue runnable;

static inline void task_queue_push(task_queue * q,task_t * task){
    int x=0;
    lock_inside(&q->lock,&x);
    task->nxt=NULL;
    if(q->tail) q->tail->nxt=task;
    else {
        Assert(q->head==NULL,"SHOULD BE NULL %p",q->head);
        q->head=task;
    }
    q->tail=task;
    unlock_inside(&q->lock,x);
    return;
}
static inline task_t * task_queue_pop(task_queue * q){
    int x=0;
    task_t * ret;
    lock_inside(&q->lock,&x);
    ret=q->head;
    if(q->head) q->head=q->head->nxt;
    if(!q->head){
        Assert(q->tail==ret,"Wrong queue %p",q);
        q->tail=NULL;
    }
    unlock_inside(&q->lock,x);
    return ret;
}

static Context * kmt_context_save(Event ev,Context * ctx){
//    Log("save_context!");
    task_t * current=current_all[cpu_current()];
//    Assert(current==NULL||current->status!=TASK_RUNABLE,"the status %d of %s SHOULD NOT be RUNNABLE!",current->status,current->name);
    if(current) current->ctx=ctx;
//    Log("%p %p",current,ctx);
    return NULL;
}

static Context * kmt_schedule(Event ev,Context * ctx){
    task_t * current=current_all[cpu_current()];
/*    if(ev.event == EVENT_SYSCALL || ev.event == EVENT_PAGEFAULT || ev.event == EVENT_ERROR) {
        Assert(current,"%p shou not be NULL!\n",current);
        return current->ctx;
    }
//    Log("Schedule!");
#ifdef LOCAL
    if(ev.event==EVENT_IRQ_TIMER) return ctx;
#endif
*/
    if(current&&current->status==TASK_RUNNING){
        current->status=TASK_RUNABLE;
        task_queue_push(&runnable,current);
    }
    if(current) {
        Assert(current->lock==1,"Unexpected lock status %d with name %s!",current->lock,current->name);
        unlock_inside_ker(&current->lock);
    }

    current=task_queue_pop(&runnable);
    while (!current||current->status!=TASK_RUNABLE){
        if(!current) Log("Current is NULL! CPU %d Waiting for the first Runnable program!",cpu_current());
        current=task_queue_pop(&runnable);
    };
    lock_inside_ker(&current->lock);
    Assert(current,"CPU%d:Current is NULL!",cpu_current());
    Assert(current->status==TASK_RUNABLE,"CPU%d: Unexpected status %d",current->status);
    current->status=TASK_RUNNING;

    current_all[cpu_current()]=current;
    Log("switch to task %s,%p",current->name,current);
    return current->ctx;
}

static void kmt_init(){
    #  define INT_MIN	(-INT_MAX - 1)
    #  define INT_MAX	2147483647
    os->on_irq(INT_MIN,EVENT_NULL,kmt_context_save);
    os->on_irq(INT_MAX, EVENT_NULL, kmt_schedule);
    task_queue_init(&runnable);

    #ifdef LOCAL
    kmt->create(task_alloc(), "tty_reader", tty_reader, "tty1");
//    kmt->create(task_alloc(), "tty_reader", tty_reader, "tty2");
    #endif
}

static int kmt_create(task_t * task, const char * name, void (*entry)(void * arg),void * arg){
    assert(task);
    task->status=TASK_RUNABLE;
    task->stack=pmm->alloc(16*4096);
    task->lock=0;
    Area temp;
    temp.start=task->stack;temp.end=(void *)((uintptr_t)task->stack+16*4096);
    task->ctx=kcontext(temp,entry,arg);
    #ifdef LOCAL
    task->name=name;
    #endif
//    Log("Task %s is added to %p",name,task);
    task_queue_push(&runnable,task);
    return 0;
}

static void kmt_teardown(task_t * task){
    pmm->free(task->stack);
    Assert(task->status==TASK_RUNABLE||task->status==TASK_RUNNING,"task %p is blocked!\n",task);
    task->status=TASK_DEAD;
    free(task->stack);
}

static void kmt_spin_init(spinlock_t *lk, const char * name){
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
    Log("Lock task name=%s",current->name);
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
    Log("Free task name=%s",nxt->name);
    task_queue_push(&runnable,nxt);
    return 0;
}

static void kmt_spin_lock(spinlock_t *lk){
    int i=0;
    lock_inside(&lk->lock,&i);
    
    if(lk->used){
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
    i=lk->status;
    unlock_inside(&lk->lock,i);
    return;
}

static void kmt_sem_init(sem_t * sem,const char * name, int value){
    sem->num=value;sem->lock=0;task_queue_init(&sem->head);
    #ifdef LOCAL
    sem->name=name;
    #endif
}

static void kmt_sem_wait(sem_t * sem){
    int i=0;
    lock_inside(&sem->lock,&i);
    sem->num--;
    if(sem->num<0) kmt_sleep(&sem->head,&sem->lock,i);
    else unlock_inside(&sem->lock,i);
}

static void kmt_sem_signal(sem_t * sem){
    int i=0;
    lock_inside(&sem->lock,&i);
    sem->num++;//Log("name=%s,left=%d",sem->name,sem->num);
    kmt_wakeup(&sem->head);
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
