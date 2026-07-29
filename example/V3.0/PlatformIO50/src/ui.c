#include "ui.h"

lv_obj_t * ui_Screen1;
lv_obj_t * ui_BackgroundImage;
lv_obj_t * ui_TempLabel;
lv_obj_t * ui_HumiLabel;
lv_obj_t * ui_OnButton;
lv_obj_t * ui_OffButton;

static ui_led_callback_t set_led;

/** Forward an On-button click to the hardware callback supplied by main.cpp. */
void ui_event_OnButton(lv_event_t * event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        set_led(true);
    }
}

/** Forward an Off-button click to the hardware callback supplied by main.cpp. */
void ui_event_OffButton(lv_event_t * event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        set_led(false);
    }
}

/** Create the generated screen and retain the application's LED callback. */
void ui_init(ui_led_callback_t led_callback)
{
    // Retain the application callback before generated widgets can emit events.
    set_led = led_callback;
    lv_display_t * display = lv_display_get_default();
    lv_theme_t * theme = lv_theme_default_init(display, lv_palette_main(LV_PALETTE_BLUE),
                                                lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_display_set_theme(display, theme);
    ui_Screen1_screen_init();
    lv_screen_load(ui_Screen1);
}
