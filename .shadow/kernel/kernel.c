#include <am.h>
#include <amdev.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <float.h>

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

extern unsigned char test_bmp[];
extern unsigned int test_bmp_len;

/**
 * 1. 将xxd得到图片像素数据
 *      需要看看是什么类型的，bmp、jpeg，不然好像用的颜色通道顺序不一样？
 *      ARGB、ABGR...
 * 2. 缩放
 * 3. 绘制图片
 */
void resize_image(const uint32_t* src_pixels, int src_width, int src_height,
                 uint32_t* dst_pixels, int dst_width, int dst_height) {
    for (int y = 0; y < dst_height; y++) {
        for (int x = 0; x < dst_width; x++) {
            // 使用最近邻插值，计算源图像中的坐标
            float src_x_ratio = (float)x / dst_width;
            float src_y_ratio = (float)y / dst_height;
            int src_x = (int)(src_x_ratio * src_width);
            int src_y = (int)(src_y_ratio * src_height);

            // 确保坐标不会超出源图像的边界
            if (src_x >= src_width - 1) src_x = src_width - 1;
            if (src_y >= src_height - 1) src_y = src_height - 1;

            // 反转 y 坐标
            src_y = src_height - 1 - src_y;

            // 从源图像中复制像素值到目标图像
            dst_pixels[y * dst_width + x] = src_pixels[src_y * src_width + src_x];
        }
    }
}
// void resize_image(const uint32_t* src_pixels, int src_width, int src_height,
//                   uint32_t* dst_pixels, int dst_width, int dst_height) {
//     for (int y = 0; y < dst_height; y++) {
//         for (int x = 0; x < dst_width; x++) {
//             int src_x = (int)(x * dst_width / src_width);
//             int src_y = (int)(y * dst_height / src_height);
// 
//             // 使用最近邻插值，直接取最接近的像素点
//             if (src_x >= src_width - 1) src_x = src_width - 1;
//             if (src_y >= src_height - 1) src_y = src_height - 1;
// 
//             // Copy the pixel value from the source image to the destination image
//             dst_pixels[y * dst_width + x] = src_pixels[src_y * src_width + src_x];
//         }
//     }
// }

void sleep() {
    for (int i = 0; i < 1000; i++) {
        printf("................\n");
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
    //printf("screen_w * screen_h * 3: %d\n", screen_w * screen_h * 4);

    uint32_t* src_pixels = (uint32_t*)malloc(src_width * src_height * 4);
    if (!src_pixels) {
        printf("Memory allocation failed\n");
        free(dst_pixels);
        return;
    }
    //printf("src_width * src_height * 3: %d\n", src_width * src_height * 4);

    // BMP shoulud be (B G R)
    for (int y = 0; y < src_height; y++) {
        for (int x = 0; x < src_width; x++) {
            int offset = y * src_width + x;
            unsigned char r = src[offset * 3];
            unsigned char g = src[offset * 3 + 1];
            unsigned char b = src[offset * 3 + 2];
            src_pixels[offset] = (r << 16) | (g << 8) | b;
            // //printf("src_pixels: %x\n", src_pixels[offset]);
        }
    }

    // 缩放图片
    resize_image(src_pixels, src_width, src_height, dst_pixels, screen_w, screen_h);

    // 绘制图片
    for (int y = 0; y < screen_h; y++) {
        for (int x = 0; x < screen_w; x++) {
            uint32_t color = dst_pixels[y * screen_w + x];
            draw_tile(x + dst_x, y + dst_y, 1, 1, color);
            //sleep();
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

//   for (int i = 0; i < test_bmp_len ; i++) {
//    printf("test_bmp[%d]: %d\n", i, test_bmp[i]);
//   }

  //splash();

  draw_image(test_bmp, 0, 0, 360, 480);

  //splash();

  puts("Press any key to see its key code...\n");
  while (1) {
    print_key();
  }
  return 0;
}
