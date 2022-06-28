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

extern task_t * current_all[8];

static inline void task_queue_push(task_queue * q,task_t * task){
    int x=0;
    lock_inside(&q->lock,&x);
    task->nxt=NULL;
    if(q->tail) q->tail->nxt=task;
    else {
        Assert(q->head==NULL,"SHOULD BE NULL %p",q->head);
        q->head=task;
    }
    q->tail=task;//show_queue(q);
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
    }//show_queue(q);
    unlock_inside(&q->lock,x);
    return ret;
}

int new_pid();
void pid_free(int pid);
extern task_t * task_all_pid[32768];
