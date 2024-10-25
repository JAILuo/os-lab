#include <am.h>
#include <amdev.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SIDE 16

static int w, h;  // Screen size

#define KEYNAME(key) \
  [AM_KEY_##key] = #key,
static const char *key_names[] = { AM_KEYS(KEYNAME) };

static inline void puts(const char *s) {
  for (; *s; s++) putch(*s);
}

void print_key() {
  AM_INPUT_KEYBRD_T event = { .keycode = AM_KEY_NONE };
  ioe_read(AM_INPUT_KEYBRD, &event);
  if (event.keycode != AM_KEY_NONE && event.keydown) {
    puts("Key pressed: ");
    puts(key_names[event.keycode]);
    puts("\n");
  }
  if (event.keydown && event.keycode == AM_KEY_ESCAPE) halt(0);
}

// draw one pixels
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

void splash() {
  AM_GPU_CONFIG_T info = {0};
  ioe_read(AM_GPU_CONFIG, &info);
  w = info.width; // 640
  h = info.height; //480

  for (int x = 0; x * SIDE <= w; x++) {
    for (int y = 0; y * SIDE <= h; y++) {
      //if ((x & 1) ^ (y & 1)) {
        //draw_tile(x * SIDE, y * SIDE, SIDE, SIDE, 0xffffff); // white
        draw_tile(x * SIDE, y * SIDE, SIDE, SIDE, 0x000000); // white
      //}
    }
  }
}

void vga_init() {
    AM_GPU_CONFIG_T info = {0};
    ioe_read(AM_GPU_CONFIG, &info);
    w = info.width; // 640
    h = info.height; //480
}

// The (0,0) is at the top-left corner of the screen
// and the order of rgb is actually bgr. https://blog.csdn.net/weixin_40437029/article/details/117530796
// This function display a half-decoded BMP image
void draw_image(const unsigned char* image_data, int image_width, int image_height) {
  AM_GPU_CONFIG_T info = {0};
  ioe_read(AM_GPU_CONFIG, &info);
  w = info.width;
  h = info.height;

  int pixel_size = 3;

  // Calculate the scaling factors
  float scale_x = (float)w / image_width;
  float scale_y = (float)h / image_height;

  // Iterate over each pixel in the new grid
  for (int y = h-1; y >= 0; y--) {
    for (int x = 0; x < w; x++) {
      // Calculate the corresponding pixel position in the original grid
      int original_x = (int)(x / scale_x);
      int original_y = (int)((h-y-1) / scale_y);

      // Get the RGB values from the original image data
      unsigned char b = image_data[(original_y * image_width + original_x) * pixel_size];
      unsigned char g = image_data[(original_y * image_width + original_x) * pixel_size + 1];
      unsigned char r = image_data[(original_y * image_width + original_x) * pixel_size + 2];

      // Combine the RGB values into a single color value
      uint32_t color = (r << 16) | (g << 8) | b;

      // Draw the pixel on the screen
      draw_tile(x, y, 1, 1, color);
    }
  }
}


void Draw_BMP(int x, int y, int w, int h, uint32_t *pixels){
    //AM_GPU_CONFIG_T info = io_read(AM_GPU_CONFIG);
    //int width = info.width;


    // for (int row = 0; row < h; row++) {
    //     //int offset = (y + row) * screen_w + x;
    //     
    //     size_t offset = (size_t)pixels + (row * w);
    //     int y = offset / width;
    //     int x = offset - y * width;

    //     AM_GPU_FBDRAW_T event = {
    //         .x = x, .y = y, .w = w, .h = 1, .sync = true,
    //         .pixels = pixels,
    //     };
    //     ioe_write(AM_GPU_FBDRAW, &event);
    // }
}


extern unsigned char test_jpg[];

// Operating system is a C program!
int main(const char *args) {
  ioe_init();

  puts("mainargs = \"");
  puts(args);  // make run mainargs=xxx
  puts("\"\n");

  splash();

  draw_image(test_jpg, 640, 480);

  //Draw_BMP(0, 0, 640, 480, (uint32_t *)test_jpg);
  //draw_pic(0, 0, 480, 640, (uint32_t *)test_jpg);

  puts("Press any key to see its key code...\n");
  while (1) {
    print_key();
  }
  return 0;
}
