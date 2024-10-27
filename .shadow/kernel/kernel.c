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

extern unsigned char test_jpg[];
extern unsigned int test_jpg_len;

/**
 * 1. 将xxd得到图片像素数据
 *      需要看看是什么类型的，bmp、jpeg，不然好像用的颜色通道顺序不一样？
 *      ARGB、ABGR...
 * 2. 缩放
 * 3. 绘制图片
 */

void resize_image(uint32_t* src_pixels, int src_width, int src_height, uint32_t* dst_pixels, int dst_width, int dst_height) {
    for (int y = 0; y < dst_height; y++) {
        for (int x = 0; x < dst_width; x++) {
            // 计算目标像素在源图像中的位置
            int src_x = (int)(x * src_width / dst_width);
            int src_y = (int)(y * src_height / dst_height);

            // 确保坐标不越界
            src_x = src_x < src_width ? src_x : src_width - 1;
            src_y = src_y < src_height ? src_y : src_height - 1;

            // 获取源图像中最近邻像素的值
            uint32_t src_color = src_pixels[src_y * src_width + src_x];
            //printf("src_x: %d  src_y: %d\n", src_x, src_y);
            //printf("src_color: %x\n\n", src_color);

            // 将值赋给目标像素
            dst_pixels[y * dst_width + x] = src_color;
            //printf("src_color: %x dst_pixels: %x\n\n", src_color, dst_pixels[y * dst_width + x]);
        }
    }
}

void draw_image(const unsigned char* src, int dst_x, int dst_y, int src_width, int src_height) {
    int screen_w, screen_h;
    get_screen_size(&screen_w, &screen_h);

  for (int i = 0; i < test_jpg_len ; i++) {
   printf("src[%d]: %x\n", i, src[i]);
  }

    // 分配内存来存储缩放后的像素数据
    uint32_t* dst_pixels = (uint32_t*)malloc(screen_w * screen_h * sizeof(uint32_t));
    if (!dst_pixels) {
        printf("Memory allocation failed\n");
        return;
    }
    printf("screen_w * screen_h * 4: %d\n", screen_w * screen_h * sizeof(uint32_t));

    // 将图片数据转换为32位ARGB格式
    uint32_t* src_pixels = (uint32_t*)malloc(src_width * src_height * sizeof(uint32_t));
    if (!src_pixels) {
        printf("Memory allocation failed\n");
        free(dst_pixels);
        return;
    }
    printf("src_width * src_height * 4: %d\n", src_width * src_height * sizeof(uint32_t));


    for (unsigned int y = 0; y < src_height; y++) {
        for (unsigned int x = 0; x < src_width; x++) {
            unsigned int offset = y * src_width;
            //printf("offset: %d\n", offset); 
            //printf("src[%d]: %d\n", offset + 3 * x, src[offset + 3 * x]);

            if (x * y > test_jpg_len) {
                break;
            }

            uint8_t r = *(((uint8_t*)&src[offset]) + 3 * x + 2);
            uint8_t g = *(((uint8_t*)&src[offset]) + 3 * x + 1);
            uint8_t b = *(((uint8_t*)&src[offset]) + 3 * x);

            //printf("r: %d g: %d b: %d\n", r, g, b);
            src_pixels[offset + x] = (r << 16) | (g << 8) | b;
            //printf("src_pixels: 0x%x\n\n", src_pixels[offset + x]);
        }
    }

    for (int i = 0; i < test_jpg_len; i++) {
        printf("src[%d]: %x\n", i, src_pixels[i]);
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

    free(src_pixels);
    free(dst_pixels);
}

// Operating system is a C program!
int main(const char *args) {
  ioe_init();

  puts("mainargs = \"");
  puts(args);  // make run mainargs=xxx
  puts("\"\n");

//   for (int i = 0; i < test_jpg_len ; i++) {
//    printf("test_jpg[%d]: %d\n", i, test_jpg[i]);
//   }

  splash();

  draw_image(test_jpg, 0, 0, 480, 640);

  //splash();

  puts("Press any key to see its key code...\n");
  while (1) {
    print_key();
  }
  return 0;
}
