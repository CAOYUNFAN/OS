#include <game.h>

#define SIDE 16
static int w, h;
static void init() {
  AM_GPU_CONFIG_T info = {0};
  ioe_read(AM_GPU_CONFIG, &info);
  w = info.width;
  h = info.height;
}
extern int posx,posy,usedx,usedy;
static void draw_tile(int x, int y, int w, int h, uint32_t color) {
  uint32_t pixels[w * h]; // WARNING: large stack-allocated memory
  AM_GPU_FBDRAW_T event = {
    .x = x, .y = y, .w = w, .h = h, .sync = 1,
    .pixels = pixels,
  };
  for (int i = 0; i < w * h; i++) {
    pixels[i] = color;
  }
  ioe_write(AM_GPU_FBDRAW, &event);
}

#define Min(a,b) ((a)<(b)?(a):(b))
int prex=-1,prey=-1;
void splash() {
  init();//printf("HELLO!\n");
  if(posx==prex&&posy==prey) return;
  if(posx<0) posx=0;
  if(posx>=w-SIDE) posx=w-SIDE;
  if(posy<0) posy=0;
  if(posy>=h-SIDE) posy=h-SIDE;
  draw_tile(posx,posy,SIDE,SIDE,0);
  usedx=usedy=0;
  if(prex!=-1&&prey!=-1) draw_tile(prex,prey,SIDE,SIDE,0xffffff);
  prex=posx;prey=posy;
/*  printf("%d %d\n",posx,posy);
  for (int x = 0; x * SIDE <= w; x ++) {
    for (int y = 0; y * SIDE <= h; y++) {
      if ((x & 1) ^ (y & 1)) {
        draw_tile(x * SIDE, y * SIDE, SIDE, SIDE, 0xffffff); // white
      }
    }
  }*/
  return;
}
void splash_init(){
  init();
  for (int x = 0; x <= w; x+=SIDE) 
    for(int y =0; y <= h; y+=SIDE) 
      draw_tile(x,y,SIDE,SIDE,0xffffff);
  return;
}
