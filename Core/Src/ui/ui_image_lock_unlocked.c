#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#elif defined(LV_LVGL_H_INCLUDE_SYSTEM)
#include <lvgl.h>
#elif defined(LV_BUILD_TEST)
#include "../lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMG_LOCK_UNLOCKED
#define LV_ATTRIBUTE_IMG_LOCK_UNLOCKED
#endif

static const
LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_LOCK_UNLOCKED
uint8_t img_lock_unlocked_map[] = {

    0xff,0xff,0xff,0xd0,0xff,0xff,0xff,0x07,

    0xff,0xff,0xf0,
    0xfe,0x07,0xf0,
    0xfd,0x07,0xf0,
    0xfc,0xfb,0xf0,
    0xfc,0xff,0xf0,
    0xfc,0xff,0xf0,
    0xf0,0x00,0xf0,
    0xf0,0x00,0xf0,
    0xf3,0xfc,0xf0,
    0xf3,0x9c,0xf0,
    0xf3,0x9c,0xf0,
    0xf3,0x9c,0xf0,
    0xf3,0x9c,0xf0,
    0xf3,0xfc,0xf0,
    0xf0,0x00,0xf0,
    0xf0,0x00,0xf0,
    0xff,0xff,0xf0,

};

const lv_image_dsc_t img_lock_unlocked = {
  .header = {
    .magic = LV_IMAGE_HEADER_MAGIC,
    .cf = LV_COLOR_FORMAT_I1,
    .flags = 0,
    .w = 20,
    .h = 17,
    .stride = 3,
    .reserved_2 = 0,
  },
  .data_size = sizeof(img_lock_unlocked_map),
  .data = img_lock_unlocked_map,
  .reserved = NULL,
};