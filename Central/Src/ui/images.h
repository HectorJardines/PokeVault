#ifndef EEZ_LVGL_UI_IMAGES_H
#define EEZ_LVGL_UI_IMAGES_H

#include <../../../Drivers/lvgl-master/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_image_dsc_t img_check;
extern const lv_image_dsc_t img_nfc;

#ifndef EXT_IMG_DESC_T
#define EXT_IMG_DESC_T
typedef struct _ext_img_desc_t {
    const char *name;
    const lv_image_dsc_t *img_dsc;
} ext_img_desc_t;
#endif

extern const ext_img_desc_t images[2];

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_IMAGES_H*/