#pragma once

/* GT911 is connected through the display carrier's I2C bus. */
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>

constexpr int kTouchSdaPin = 19;
constexpr int kTouchSclPin = 20;
constexpr int kTouchInterruptPin = 15;
constexpr int kTouchResetPin = 38;
constexpr int kTouchMapXMin = 0;
constexpr int kTouchMapXMax = 800;
constexpr int kTouchMapYMin = 0;
constexpr int kTouchMapYMax = 480;

TAMC_GT911 touch_controller(kTouchSdaPin, kTouchSclPin, kTouchInterruptPin, kTouchResetPin,
                            max(kTouchMapXMin, kTouchMapXMax), max(kTouchMapYMin, kTouchMapYMax));

int touch_last_x = 0;
int touch_last_y = 0;

/**
 * @brief Initialize the GT911 bus and set normal display orientation.
 */
void touch_init()
{
    Wire.begin(kTouchSdaPin, kTouchSclPin);
    touch_controller.begin();
    touch_controller.setRotation(ROTATION_NORMAL);
}

/**
 * @brief Return whether the input driver should attempt a touch read.
 *
 * The GT911 implementation is polled, so this remains true each cycle.
 */
bool touch_has_signal()
{
    return true;
}

/**
 * @brief Read the first GT911 point and map it to display coordinates.
 *
 * @return true while a finger is detected; false when no point is active.
 */
bool touch_touched()
{
    touch_controller.read();
    if (!touch_controller.isTouched) {
        return false;
    }

    touch_last_x = map(touch_controller.points[0].x, kTouchMapXMin, kTouchMapXMax, 0,
                       lcd->width() - 1);
    touch_last_y = map(touch_controller.points[0].y, kTouchMapYMin, kTouchMapYMax, 0,
                       lcd->height() - 1);

    touch_last_x = lcd->width() - 1 - touch_last_x;
    touch_last_y = lcd->height() - 1 - touch_last_y;
    return true;
}
