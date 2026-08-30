#include "ui.h"
#include "screens.h"
#include "images.h"
#include "actions.h"
#include "vars.h"

#include <string.h>

static int16_t currentScreen = -1;
static lv_obj_t *small_screen;

static void reparent_children(lv_obj_t *screen);
static lv_obj_t *getLvglObjectFromIndex(int32_t index);



void loadScreen(enum ScreensEnum screenId) {
    lv_obj_t *old = lv_screen_active();
    lv_screen_load(small_screen);
    reparent_children(old);
    lv_obj_delete(old);
    uint32_t delay = 0;

    currentScreen = screenId - 1;
    switch (screenId)
    {
    case SCREEN_ID_ADD_ITEM:
        create_screen_add_item();
        break;
    case SCREEN_ID_INVENTORY:
        create_screen_inventory();
        break;
    case SCREEN_ID_MAIN:
        create_screen_main();
        break;
    case SCREEN_ID_SCAN_PROMPT:
        create_screen_scan_prompt();
        break;
    case SCREEN_ID_SCANNED:
        create_screen_scanned();
        break;
    case SCREEN_ID_SCANNING:
        create_screen_scanning();
        break;
    default:
        break;
    }

    lv_obj_t *screen = getLvglObjectFromIndex(currentScreen);
    lv_screen_load_anim(screen, LV_SCREEN_LOAD_ANIM_NONE, 0, delay, false);
}

void ui_init() {
    // temporary blank screen to load in when swapping out screens
    small_screen = lv_obj_create(0);
    create_screens();
    lv_obj_t *screen = getLvglObjectFromIndex(SCREEN_ID_MAIN - 1);
    lv_screen_load_anim(screen, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, false);
}

void ui_tick() {
    tick_screen(currentScreen);
}


/**
 * @brief Reparents all children of the screen to flatten display structure
 * 
 * 
 * This function should be called before deleting a screen
 * to eliminate the recursive depth of the lv_obj_delete 
 * function.
 *
 */
static void reparent_children(lv_obj_t *screen) {

}



static lv_obj_t *getLvglObjectFromIndex(int32_t index) {
    if (index == -1) {
        return 0;
    }
    return ((lv_obj_t **)&objects)[index];
}
