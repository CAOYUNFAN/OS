#include <common.h>

static void os_init() {
  pmm->init();
  kmt->init();
}

static void os_run() {
  #ifdef LOCAL
  for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
    putch(*s == '*' ? '0' + cpu_current() : *s);
  }
  #endif
  iset(true);
  while (1) ;
}

typedef struct event_local{
  int seq;
  int event;
  handler_t handler;
  struct event_local * pre, * nxt;
}event_local_t;

event_local_t * start=NULL;

static Context * os_trap(Event ev, Context * context){
  Context *next = NULL;
  for (event_local_t *h=start;h;h=h->nxt) {
    if (h->event == EVENT_NULL || h->event == ev.event) {
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
