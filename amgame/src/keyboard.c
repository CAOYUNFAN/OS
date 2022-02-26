#include <game.h>

#define KEYNAME(key) \
  [AM_KEY_##key] = #key,
static const char *key_names[] = {
  AM_KEYS(KEYNAME)
};
int posx,posy;
void func_key() {
  AM_INPUT_KEYBRD_T event = { .keycode = AM_KEY_NONE };
  ioe_read(AM_INPUT_KEYBRD, &event);
  if (event.keycode != AM_KEY_NONE && event.keydown) {
    if(event.keycode==1) halt(0);
    if(strcmp(key_names[event.keycode],"A")==0) posy-=16;
    if(event.keycode==43) posx-=16;
    if(event.keycode==44) posy+=16;
    if(event.keycode==45) posx+=16;
  }
}
/*
puts("Key pressed: ");
    printf("%d %s",event.keycode,key_names[event.keycode]);
    puts("\n");
*/