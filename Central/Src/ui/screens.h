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
    SCREEN_ID_INVENTORY = 2,
    SCREEN_ID_ADD_ITEM = 3,
    SCREEN_ID_SCANNING = 4,
    SCREEN_ID_SCANNED = 5,
    _SCREEN_ID_LAST = 5
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *inventory;
    lv_obj_t *add_item;
    lv_obj_t *scanning;
    lv_obj_t *scanned;
    lv_obj_t *obj0;
    lv_obj_t *node_name_6;
    lv_obj_t *node_status_6;
    lv_obj_t *node_cap_6;
    lv_obj_t *node_1;
    lv_obj_t *node_name;
    lv_obj_t *node_status;
    lv_obj_t *node_cap;
    lv_obj_t *node_2;
    lv_obj_t *node_name_1;
    lv_obj_t *node_status_1;
    lv_obj_t *node_cap_1;
    lv_obj_t *node_3;
    lv_obj_t *node_name_2;
    lv_obj_t *node_status_2;
    lv_obj_t *node_cap_2;
    lv_obj_t *node_4;
    lv_obj_t *node_name_3;
    lv_obj_t *node_status_3;
    lv_obj_t *node_cap_3;
    lv_obj_t *node_5;
    lv_obj_t *node_name_4;
    lv_obj_t *node_status_4;
    lv_obj_t *node_cap_4;
    lv_obj_t *node_6;
    lv_obj_t *node_name_5;
    lv_obj_t *node_status_5;
    lv_obj_t *node_cap_5;
    lv_obj_t *item_add;
    lv_obj_t *add_lb;
    lv_obj_t *invent_header;
    lv_obj_t *name_header;
    lv_obj_t *node_status_7;
    lv_obj_t *node_cap_7;
    lv_obj_t *products;
    lv_obj_t *product_1;
    lv_obj_t *name;
    lv_obj_t *tmp1;
    lv_obj_t *tmp2;
    lv_obj_t *product_2;
    lv_obj_t *name_1;
    lv_obj_t *tmp1_1;
    lv_obj_t *tmp2_1;
    lv_obj_t *product_3;
    lv_obj_t *name_2;
    lv_obj_t *tmp1_2;
    lv_obj_t *tmp2_2;
    lv_obj_t *product_4;
    lv_obj_t *name_3;
    lv_obj_t *tmp1_3;
    lv_obj_t *tmp2_3;
    lv_obj_t *product_5;
    lv_obj_t *name_4;
    lv_obj_t *tmp1_4;
    lv_obj_t *tmp2_4;
    lv_obj_t *product_6;
    lv_obj_t *name_5;
    lv_obj_t *tmp1_5;
    lv_obj_t *tmp2_5;
    lv_obj_t *product_7;
    lv_obj_t *name_6;
    lv_obj_t *tmp1_6;
    lv_obj_t *tmp2_6;
    lv_obj_t *invent_btns;
    lv_obj_t *back_btn;
    lv_obj_t *back_label;
    lv_obj_t *forward_btn;
    lv_obj_t *forward_label;
    lv_obj_t *home_btn;
    lv_obj_t *home_label;
    lv_obj_t *kb1;
    lv_obj_t *input_area;
    lv_obj_t *txt_ar_lb;
    lv_obj_t *back_btn_add;
    lv_obj_t *back;
    lv_obj_t *spn_scanning;
    lv_obj_t *scan_label;
    lv_obj_t *lbl_scn_cplt;
    lv_obj_t *img_check;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void create_screen_inventory();
void tick_screen_inventory();

void create_screen_add_item();
void tick_screen_add_item();

void create_screen_scanning();
void tick_screen_scanning();

void create_screen_scanned();
void tick_screen_scanned();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/