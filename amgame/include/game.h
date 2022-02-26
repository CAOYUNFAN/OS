#include <am.h>
#include <amdev.h>
#include <klib.h>
#include <klib-macros.h>

void splash();
void func_key();
static inline void puts(const char *s) {
  for (; *s; s++) putch(*s);
}
