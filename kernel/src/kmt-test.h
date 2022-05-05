#ifdef LOCAL
#include <devices.h>
void tty_reader(void *arg) {
  putch('a');
  device_t *tty = dev->lookup(arg);
  putch('a');
  char cmd[128], resp[128], ps[16];
  snprintf(ps, 16, "(%s) $ ", arg);
  putch('a');
  while (1) {
    putch('a');
    tty->ops->write(tty, 0, ps, strlen(ps));
    int nread = tty->ops->read(tty, 0, cmd, sizeof(cmd) - 1);
    cmd[nread] = '\0';
    sprintf(resp, "tty reader task: got %d character(s).\n", strlen(cmd));
    tty->ops->write(tty, 0, resp, strlen(resp));
  }
}
static inline task_t *task_alloc() {
  return pmm->alloc(sizeof(task_t));
}
#endif
