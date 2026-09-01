#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <../../../Drivers/lvgl-master/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_back_to_main(lv_event_t * e);
extern void action_next_items(lv_event_t * e);
extern void action_previous_items(lv_event_t * e);
extern void action_register_prompt(lv_event_t * e);
extern void action_to_inventory(lv_event_t * e);
extern void action_scan_prompt(lv_event_t * e);
extern void action_next_units(lv_event_t *e);
extern void action_prev_units(lv_event_t *e);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/