#include <am.h>
#include <amdev.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SIDE 16

//static int w, h;  // Screen size

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

// void splash() {
//   // AM_GPU_CONFIG_T info = {0};
//   // ioe_read(AM_GPU_CONFIG, &info);
//   // w = info.width; // 640
//   // h = info.height; //480
//   
//   int w, h;
//   get_screen_size(&w, &h);
// 
//   for (int x = 0; x * SIDE <= w; x++) {
//     for (int y = 0; y * SIDE <= h; y++) {
//       if ((x & 1) ^ (y & 1)) {
//         draw_tile(x * SIDE, y * SIDE, SIDE, SIDE, 0xffffff); // white
//       }
//     }
//   }
// }

extern unsigned char test_jpg[];
extern unsigned int test_jpg_len;

/**
 * 1. 将xxd得到图片像素数据
 *      需要看看是什么类型的，bmp、jpeg，不然好像用的颜色通道顺序不一样？
 *      ARGB、ABGR...
 * 2. 缩放
 * 3. 绘制图片
 */

void resize_image(const uint32_t* src_pixels, int src_width, int src_height,
                  uint32_t* dst_pixels, int dst_width, int dst_height) {
    float x_scale = (float)src_width / dst_width;
    float y_scale = (float)src_height / dst_height;
    printf("src_width: %d  src_height: %d\n", src_width, src_height);
    printf("x_scale: %f  y_scale: %f\n", x_scale, y_scale);

    for (int y = 0; y < dst_height; y++) {
        for (int x = 0; x < dst_width; x++) {
            // Calculate the corresponding position in the source image
            int src_x = (int)(x * x_scale);
            int src_y = (int)(y * y_scale);

            // 使用最近邻插值，直接取最接近的像素点
            if (src_x >= src_width - 1) src_x = src_width - 1;
            if (src_y >= src_height - 1) src_y = src_height - 1;

            // Copy the pixel value from the source image to the destination image
            dst_pixels[y * dst_width + x] = src_pixels[src_y * src_width + src_x];
        }
    }
}

// void resize_image(const uint32_t* src_pixels, int src_width, int src_height,
//                   uint32_t* dst_pixels, int dst_width, int dst_height) {
//     float x_scale = (float)src_width / dst_width;
//     float y_scale = (float)src_height / dst_height;
// 
//     for (int y = 0; y < dst_height; y++) {
//         for (int x = 0; x < dst_width; x++) {
//             // Calculate the corresponding position in the source image
//             int src_x = (int)(x * x_scale);
//             int src_y = (int)(y * y_scale);
// 
//             // Copy the pixel value from the source image to the destination image
//             dst_pixels[y * dst_width + x] = src_pixels[src_y * src_width + src_x];
//         }
//     }
// }

void draw_image(const unsigned char* src, 
                int dst_x, int dst_y, int src_width, int src_height) {
    int screen_w, screen_h;
    get_screen_size(&screen_w, &screen_h);

    // 分配内存来存储缩放后的像素数据
    uint32_t* dst_pixels = (uint32_t*)malloc(screen_w * screen_h * sizeof(uint32_t));
    if (!dst_pixels) {
        printf("Memory allocation failed\n");
        return;
    }

    // 将图片数据转换为32位ARGB格式
    uint32_t* src_pixels = (uint32_t*)malloc(src_width * src_height * sizeof(uint32_t));
    if (!src_pixels) {
        printf("Memory allocation failed\n");
        free(dst_pixels);
        return;
    }
    for (int y = 0; y < src_height; y++) {
        for (int x = 0; x < src_width; x++) {
            int offset = y * src_width + x;
            unsigned char r = src[offset * 3];
            unsigned char g = src[offset * 3 + 1];
            unsigned char b = src[offset * 3 + 2];
            src_pixels[offset] = (0xFF << 24) | (r << 16) | (g << 8) | b;
            printf("src_pixels: %x\n", src_pixels[offset]);
        }
    }

    // 缩放图片
    resize_image(src_pixels, src_width, src_height, dst_pixels, screen_w, screen_h);

    // 绘制图片
    for (int y = 0; y < screen_h; y++) {
        for (int x = 0; x < screen_w; x++) {
            uint32_t color = dst_pixels[y * screen_w + x];
            draw_tile(dst_x + x, dst_y + y, 1, 1, color);
        }
    }

    // 释放内存
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

  draw_image(test_jpg, 0, 0, 320, 240);

  puts("Press any key to see its key code...\n");
  while (1) {
    print_key();
  }
  return 0;
}
