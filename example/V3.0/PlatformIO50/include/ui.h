#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_led_callback_t)(bool enabled);

/* Generated objects exported for application code and event callbacks. */

extern lv_obj_t * ui_Screen1;
extern lv_obj_t * ui_BackgroundImage;
extern lv_obj_t * ui_TempLabel;
extern lv_obj_t * ui_HumiLabel;
extern lv_obj_t * ui_OnButton;
extern lv_obj_t * ui_OffButton;

LV_IMAGE_DECLARE(ui_img_background_png);
LV_IMAGE_DECLARE(ui_img_on_png);
LV_IMAGE_DECLARE(ui_img_off_png);

void ui_Screen1_screen_init(void);
void ui_event_OnButton(lv_event_t * event);
void ui_event_OffButton(lv_event_t * event);
void ui_init(ui_led_callback_t led_callback);

#ifdef __cplusplus
}
#endif
