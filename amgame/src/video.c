#include <game.h>

#define SIDE 16
static int w, h;
static void init() {
  AM_GPU_CONFIG_T info = {0};
  ioe_read(AM_GPU_CONFIG, &info);
  w = info.width;
  h = info.height;
}
extern int posx,posy;
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
void splash() {
  init();printf("HELLO!\n");
  if(posx<0) posx=0;
  if(posx>=w-4*SIDE) posx=w-4*SIDE;
  if(posy<0) posy=0;
  if(posy>=h-4*SIDE) posy=h-4*SIDE;
  draw_tile(posx,posy,4*SIDE,4*SIDE,0xffffff);
  printf("%d %d\n",posx,posy);
  /*for (int x = 0; x * SIDE <= w; x ++) {
    for (int y = 0; y * SIDE <= h; y++) {
      if ((x & 1) ^ (y & 1)) {
        draw_tile(x * SIDE, y * SIDE, SIDE, SIDE, 0xffffff); // white
      }
    }
  }*/
}
