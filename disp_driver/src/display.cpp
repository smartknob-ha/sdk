#include "display.hpp"

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#include "lvgl.h"
#include "lvgl_helpers.h"

void Display::lvgl_timer_task(void *arg)
{
    lv_tick_inc(CONFIG_LVGL_TICK_RATE);
}

void Display::lvgl_gui_task(void *arg)
{
    ESP_LOGD(TAG, "LVGL GUI task started");

#ifdef CONFIG_DISPLAY_FRAME_BUFFER_PSRAM
    static lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    static lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
#else
    static lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_INTERNAL);
    static lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_INTERNAL);
#endif

    lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISP_BUF_SIZE);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    const esp_timer_create_args_t timer_args = {
        .callback = &lvgl_timer_task,
        .name = "lvgl_timer"};
    esp_timer_handle_t timer_handle;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle, CONFIG_LVGL_TICK_RATE * 1000)); // Function expects time period in microseconds

    while (true)
    {
        uint32_t next_delay = lv_timer_handler();
        if (next_delay == 0)
            continue;
        vTaskDelay(portTICK_PERIOD_MS / next_delay);
    }
}

esp_err_t Display::init(uint8_t bl_brightness)
{
    lv_init();

    lvgl_driver_init();

    const disp_backlight_config_t bl_config = {
        .pwm_control = true,
        .output_invert = false,
        .gpio_num = BACKLIGHT_GPIO,
        .timer_idx = 0,
        .channel_idx = 0};

    // So logs can be directed to centralized logging component
    lv_log_register_print_cb(lvgl_log);

    xTaskCreatePinnedToCore(lvgl_gui_task, "gui", 4096, NULL, 0, NULL, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // Only initialize backlight after LVGL init so that the initial garbage isn't shown
    bl_handle = disp_backlight_new(&bl_config);
    set_bl_brightness(bl_brightness);

    return ESP_OK;
}

void Display::lvgl_log(const char *log)
{
    // TODO replace with calls to logging component once existent
    printf(log);
    fflush(stdout);
}

void Display::set_bl_brightness(uint8_t brightness)
{
    disp_backlight_set(bl_handle, brightness);
}