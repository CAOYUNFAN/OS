#include <common.h>

#ifdef LOCAL
sem_t empty, fill;
#define P kmt->sem_wait
#define V kmt->sem_signal
void producer(void *arg) { while (1) { P(&empty); putch('(');/*putch('\n');*/ V(&fill);  } }
void consumer(void *arg) { while (1) { P(&fill);  putch(')');/*putch('\n');*/ V(&empty); } }
static inline task_t *task_alloc() {
  return pmm->alloc(sizeof(task_t));
}
#endif


static void os_init() {
  pmm->init();
  kmt->init();printf("A\n");
  uproc->init();
  #ifdef LOCAL
//  dev->init();
/*  static char ch1[5][20],ch2[5][20];
  kmt->sem_init(&empty, "empty", 5);  // 缓冲区大小为 5
  kmt->sem_init(&fill,  "fill",  0);
  for (int i = 0; i < 2; i++){
    sprintf(ch1[i],"producer-%d",i);
    kmt->create(task_alloc(), ch1[i], producer, NULL);
  } // 4 个生产者
  for (int i = 0; i < 2; i++){
    sprintf(ch2[i],"consumer-%d",i);
    kmt->create(task_alloc(), ch2[i], consumer, NULL);
  } // 5 个消费者*/
  #endif
}

static void os_run() {
  #ifdef LOCAL
  for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
    putch(*s == '*' ? '0' + cpu_current() : *s);
  }
  #endif
  iset(true);
  yield();
  panic_on(1,"should not reach here!\n");
}

typedef struct event_local{
  int seq;
  int event;
  handler_t handler;
  struct event_local * pre, * nxt;
}event_local_t;

event_local_t * start=NULL;
extern task_t * current_all[8];

static Context * os_trap(Event ev, Context * context){
  Context *next = NULL;
  assert(!ienabled());
//  Log("%s",ev.msg);
  int flag=(current_all[cpu_current()]->status!=TASK_DEAD);
  for (event_local_t *h=start;h;h=h->nxt) {
    if (h->event == EVENT_NULL || (h->event == ev.event && flag)) {
//      Log("In function %p",h->handler);
      Context *r = h->handler(ev, context);
      panic_on(r && next, "returning multiple contexts");
      if (r) next = r;
    }
  }
  panic_on(!next, "returning NULL context");
//  panic_on(sane_context(next), "returning to invalid context");
  return next;
}

static void os_on_irq(int seq,int event,handler_t handler){
//  Log("Insert function %p",handler);
  event_local_t ** now=&start;
  while(*now&&(*now)->seq<seq) now=&((*now)->nxt);
  event_local_t * temp=pmm->alloc(sizeof(event_local_t));
  temp->seq=seq;temp->event=event;temp->handler=handler;
  temp->nxt=*now;
  *now=temp;
  return;
}

MODULE_DEF(os) = {
  .init = os_init,
  .run  = os_run,
  .trap = os_trap,
  .on_irq = os_on_irq
};
