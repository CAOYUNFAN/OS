#include <game.h>

#define KEYNAME(key) \
  [AM_KEY_##key] = #key,
static const char *key_names[] = {
  AM_KEYS(KEYNAME)
};
int posx=0,posy=0,usedx=0,usedy=0;
void func_key() {
  AM_INPUT_KEYBRD_T event = { .keycode = AM_KEY_NONE };
  ioe_read(AM_INPUT_KEYBRD, &event);
  if (event.keycode != AM_KEY_NONE && event.keydown) {
    if(event.keycode==1) halt(0);
    if(*key_names[event.keycode]=='A'&&!usedy) posy-=16,usedy=1;
    if(event.keycode==43&&!usedx) posx-=16,usedx=1;
    if(event.keycode==44&&!usedy) posy+=16,usedy=1;
    if(event.keycode==45&&!usedx) posx+=16,usedx=1;
//    printf("%s\n",key_names[event.keycode]);
  }
}
/*
puts("Key pressed: ");
    printf("%d %s",event.keycode,key_names[event.keycode]);
    puts("\n");
*/