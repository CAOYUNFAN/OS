#include <os.h>

static inline void lock_inside(int * addr,int * status){
    *status=ienable();
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

inline void init_list(list_head * list){
    list->head=NULL;list->lock=0;
    return;
}
list_head runnable,running;

void add_list(list_head * list,task_t * task){
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
void del_list(list_head * list,task_t * task){
    int x=0;
    lock_inside(&list->lock,&x);
    if(list->head==task) {
        if(list->head==task->nxt) list->head=NULL;
        else list->head=task->nxt;
    }
    task->nxt->pre=task->pre;task->pre->nxt=task->nxt;
    unlock_inside(&list->lock,x);
    return;
}
task_t * del_list2(list_head * list){
    int x=0;
    lock_inside(&list->lock,&x);
    task_t * task=list->head;
    if(list->head==task->nxt) list->head=NULL;
    else list->head=task->nxt;
    task->nxt->pre=task->pre;task->pre->nxt=task->nxt;
    unlock_inside(&list->lock,x);
    return task;
}

static Context * kmt_context_save(Event ev,Context * ctx){
    task_t * current=current_all[cpu_current()];
    Assert(current==NULL||current->status!=TASK_RUNABLE,"the status of %p SHOULD NOT be RUNNABLE!",current);
    if(current) current->ctx=ctx;
    return NULL;
}

static Context * kmt_schedule(Event ev,Context * ctx){
    task_t * current=current_all[cpu_current()];
    if(current&&current->status==TASK_RUNNING){
        del_list(&running,current);
        current->status=TASK_RUNABLE;
        add_list(&runnable,current);
    }
    int pp=0;
    lock_inside(&runnable.lock,&pp);
    current=runnable.head;

    while (!current){
        unlock_inside(&runnable.lock);
        Log("Current is NULL! CPU %d Waiting for the first Runnable program!",cpu_current());
        lock_inside(&runnable.lock);
        current=runnable.head;
    };

    Assert(current,"CPU%d:Current is NULL!",cpu_current());
    Assert(current->status==TASK_RUNABLE,"CPU%d: Unexpected status %d",current->status);
    current->status=TASK_RUNNING;

    unlock_inside(&runnable.head,pp);
    current_all[cpu_current()]=current;
    return current->status;
}

static void kmt_init(){
    #  define INT_MIN	(-INT_MAX - 1)
    #  define INT_MAX	2147483647
    os->on_irq(INT_MIN,EVENT_NULL,kmt_context_save);
    os->on_irq(INT_MAX, EVENT_NULL, kmt_schedule);
    init_list(&runnable);
    init_list(&running);
    assert(0);
}

static int kmt_create(task_t * task, const char * name, void (*entry)(void * arg),void * arg){
    assert(task);
    task->status=TASK_RUNABLE;
    add_list(&runnable,task);
    return 0;
}

static void kmt_teardown(task_t * task){
    Assert(task->status==TASK_RUNABLE||task_status==TASK_RUNNING,"task %p is blocked!\n",task);
    if(task->status==TASK_RUNABLE) del_list(&runnable,task);
    else{
        del_list(&running,task);
        task->status=TASK_DEAD;
        yield();
    }
}

static void kmt_spin_init(spinlock_t *lk, const char * name){
    init_list(&lk->head);lk->used=0;
    return;
}

static void kmt_spin_lock(spinlock_t *lk){
    if(atomic_xchg(&lk->used,1)==0){
        task_t * current=current_all[cpu_current()];
        del_list(&running,current);
        Assert(current->status==TASK_RUNNING,"Unexpected task status %d\n",current->status);
        current->status=TASK_WAITING;
        add_list(&lk->head,current);
        yield();
        Assert(current->status==TASK_RUNNING,"Unexpected task status %d\n",current->status);
    }
    return;
}

static void kmt_spin_unlock(spinlock_t * lk){
    int x=0;lock_inside(&lk->head.lock,&x);
    if(lk->head.head==NULL) atomic_xchg(&lk->used,0);
    else{
        task_t * task=del_list2(&lk->head);
        Assert(current->status==TASK_WAITING,"Unexpected task status %d\n",current->status);
        task->status=TASK_RUNABLE;
        add_list(&runnable,task);
    }
    return;
}

static void kmt_sem_init(sem_t * sem,const char * name, int value){
    assert(0);
}

static void kmt_sem_wait(sem_t * sem){
    assert(0);
}

static void kmt_sem_signal(sem_t * sem){
    assert(0);
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
