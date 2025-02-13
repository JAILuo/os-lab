#include <am.h>
#include <amdev.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <float.h>
#include "../L0/image.h"

#define SIDE 16

//static int w, h;  // Screen size

#define BMP_FILE_HEADER_SIZE 0x36

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

void get_screen_size(int *w, int *h) {
    AM_GPU_CONFIG_T info = {0};
    ioe_read(AM_GPU_CONFIG, &info);
    *w = info.width; // 640
    *h = info.height; //480
}

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
  // AM_GPU_CONFIG_T info = {0};
  // ioe_read(AM_GPU_CONFIG, &info);
  // w = info.width; // 640
  // h = info.height; //480
  
  int w, h;
  get_screen_size(&w, &h);

  for (int x = 0; x * SIDE <= w; x++) {
    for (int y = 0; y * SIDE <= h; y++) {
      if ((x & 1) ^ (y & 1)) {
        draw_tile(x * SIDE, y * SIDE, SIDE, SIDE, 0xffffff); // white
      }
    }
  }
}

extern unsigned char test2_bmp[];

/**
 * 最初的思考过程
 * 1. 用xxd得到图片像素数据
 *      需要看看是什么类型的，bmp、jpeg，不然好像用的颜色通道顺序不一样，还要解码？
 *      ARGB、ABGR...
 *      得到的数据是怎么样的？需要我再进行处理吗？
 *      还要怎么处理？
 * 2. 缩放
 *      具体算法种类？
 * 3. 绘制图片
 *     基本的基于行的写像素 
 */
void resize_image(const uint32_t* src_pixels, int src_width, int src_height,
                 uint32_t* dst_pixels, int dst_width, int dst_height) {
    for (int y = 0; y < dst_height; y++) {
        for (int x = 0; x < dst_width; x++) {
            // Nearest Neighbor interpolation
            float src_x_ratio = (float)x / dst_width;
            float src_y_ratio = (float)y / dst_height;
            int src_x = (int)(src_x_ratio * src_width);
            int src_y = (int)(src_y_ratio * src_height);

            if (src_x >= src_width - 1) src_x = src_width - 1;
            if (src_y >= src_height - 1) src_y = src_height - 1;

            // Invert the y-coordinates
            // The bmp image starts at the last line and is scanned upwards progressively
            src_y = src_height - 1 - src_y;

            dst_pixels[y * dst_width + x] = src_pixels[src_y * src_width + src_x];
        }
    }
}

void draw_image(const unsigned char* src, int dst_x, int dst_y, int src_width, int src_height) {
    int screen_w, screen_h;
    get_screen_size(&screen_w, &screen_h);

    // 640 * 480 * 4 = 1228800 ~= 1.17 MiB 
    uint32_t* dst_pixels = (uint32_t*)malloc(screen_w * screen_h* 4);
    if (!dst_pixels) {
        printf("Memory allocation failed\n");
        return;
    }
    //printf("screen_w * screen_h * 4: %d\n", screen_w * screen_h * 4);

    uint32_t* src_pixels = (uint32_t*)malloc(src_width * src_height * 4);
    if (!src_pixels) {
        printf("Memory allocation failed\n");
        free(dst_pixels);
        return;
    }
    //printf("src_width * src_height * 4: %d\n", src_width * src_height * 4);

    src = (uint8_t *)src + BMP_FILE_HEADER_SIZE; // jump BMP file header 

    // Padding bytes per row
    // int line_padding = ((src_width * 3 + 31) & ~31) - (src_width * 3);
    // int line_padding = (4 - (src_width * 3) % 4) % 4;
    // The above is GPT generated, but can be used
    int line_padding = ((src_width & 32) + 31) & ~31;

    for (int y = src_height - 1; y >= 0; y--) {
        for (int x = src_width - 1; x >= 0; x--) {
            // hexedit xx.bmp, read biBitCount: 0x0020 = 32 bits = 4 bytes <-> 1 pixels
            int src_index = (y * (src_width * 4 + line_padding)) + (x * 4);
            
            // BMP shoulud be (B G R)
            // little-endian, (low addr) B-G-R (high addr)
            unsigned char b = src[src_index + 0];
            unsigned char g = src[src_index + 1];
            unsigned char r = src[src_index + 2];

            int offset = y * src_width + x;
            src_pixels[offset] = (0xff000000) | (r << 16) | (g << 8) | b;

            //printf("src_index: %d\n", src_index);
            //printf("offset: %d\n\n", offset);
        }
        src += line_padding; // jump line padding
    }

    resize_image(src_pixels, src_width, src_height, dst_pixels, screen_w, screen_h);

    // Actual drawing
    for (int y = screen_h; y >= 0; y--) {
        for (int x = 0; x < screen_w; x++) {
            uint32_t color = dst_pixels[y * screen_w + x];
            draw_tile(x + dst_x, y + dst_y, 1, 1, color);
        }
    }

    free(src_pixels);
    free(dst_pixels);
}

// Operating system is a C program!
int main(const char *args) {
  ioe_init();

  puts("mainargs = \"");
  puts(args);  // make run mainargs=xxx
  puts("\"\n");

  //splash();

  draw_image(test2_bmp, 0, 0, 640, 427);

  puts("Press any key to see its key code...\n");
  while (1) {
    print_key();
  }
  return 0;
}
