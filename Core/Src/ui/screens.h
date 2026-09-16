#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <../../../Drivers/lvgl-master/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_ITEM_SCANNING = 2,
    SCREEN_ID_ITEM_SCANNED = 3,
    SCREEN_ID_ITEM_SCAN_FAILED = 4,
    _SCREEN_ID_LAST = 4
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *item_scanning;
    lv_obj_t *item_scanned;
    lv_obj_t *item_scan_failed;
    lv_obj_t *label_armed_status;
    lv_obj_t *label_armed_status_val;
    lv_obj_t *label_temp;
    lv_obj_t *label_hum;
    lv_obj_t *panel_temp;
    lv_obj_t *label_temp_val;
    lv_obj_t *panel_hum;
    lv_obj_t *label_hum_val;
    lv_obj_t *spn_scanning;
    lv_obj_t *lbl_scanning;
    lv_obj_t *lbl_scan_cplt;
    lv_obj_t *img_check;
    lv_obj_t *obj0;
    lv_obj_t *obj1;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void create_screen_item_scanning();
void tick_screen_item_scanning();

void create_screen_item_scanned();
void tick_screen_item_scanned();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/