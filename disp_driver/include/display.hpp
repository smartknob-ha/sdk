#ifndef DISPLAY_HPP_
#define DISPLAY_HPP_

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "lvgl_helpers.h"

#define BACKLIGHT_GPIO      7

class Display
{
private:
    static inline char *TAG = "DISPLAY";

    static void lvgl_timer_task(void *arg);
    static void lvgl_gui_task(void *arg);
    static void lvgl_log(const char* log);

    disp_backlight_h bl_handle;

public:
    void set_bl_brightness(uint8_t brightness);
    esp_err_t init(uint8_t bl_brightness);
    void status();
};

#endif // DISPLAY_HPP_