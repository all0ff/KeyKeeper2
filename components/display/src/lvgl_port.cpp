#include "display/lvgl_port.hpp"

#include "display/display.hpp"
#include "display/display_config.hpp"
#include "display_panel.hpp"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "lvgl.h"

namespace lvgl_port {

namespace {

constexpr char TAG[] = "lvgl_port";

bool initialized = false;

lv_display_t* lv_disp = nullptr;

uint16_t* draw_buf_1 = nullptr;
uint16_t* draw_buf_2 = nullptr;

SemaphoreHandle_t lvgl_mutex = nullptr;
esp_timer_handle_t tick_timer = nullptr;
TaskHandle_t lvgl_task_handle = nullptr;

// -------------------------------------------------------------------
// LVGL flush callback
// -------------------------------------------------------------------
//
// Called by LVGL whenever a rendered area needs to be pushed to the
// panel. Runs on the LVGL task, with the LVGL lock already held by
// the caller (lv_timer_handler()).

void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map)
{
    auto& lcd = display::internal::lcd();

    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;

    lcd.startWrite();
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    lcd.writePixels(reinterpret_cast<lgfx::rgb565_t*>(px_map), static_cast<uint32_t>(w) * h);
    lcd.endWrite();

    lv_display_flush_ready(disp);
}

// -------------------------------------------------------------------
// LVGL tick source
// -------------------------------------------------------------------

void tick_timer_cb(void* /*arg*/)
{
    lv_tick_inc(display::params::Lvgl::tick_period_ms);
}

bool start_tick_timer()
{
    const esp_timer_create_args_t args{
        .callback = &tick_timer_cb,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lvgl_tick",
        .skip_unhandled_events = true,
    };

    esp_err_t err = esp_timer_create(&args, &tick_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LVGL tick timer: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_timer_start_periodic(
        tick_timer,
        static_cast<uint64_t>(display::params::Lvgl::tick_period_ms) * 1000);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start LVGL tick timer: %s", esp_err_to_name(err));
        esp_timer_delete(tick_timer);
        tick_timer = nullptr;
        return false;
    }

    return true;
}

// -------------------------------------------------------------------
// LVGL task
// -------------------------------------------------------------------

void lvgl_task(void* /*arg*/)
{
    const TickType_t period = pdMS_TO_TICKS(display::params::Lvgl::task_period_ms);

    while (true) {
        lock();
        const uint32_t delay_ms = lv_timer_handler();
        unlock();

        // Respect LVGL's requested delay, but never wait less than our
        // configured task period, and never longer than it either --
        // this task only needs to be responsive, not exact.
        (void)delay_ms;
        vTaskDelay(period);
    }
}

// -------------------------------------------------------------------
// Draw buffer allocation
// -------------------------------------------------------------------

bool allocate_draw_buffers(size_t pixels_per_buffer)
{
    const size_t bytes = pixels_per_buffer * sizeof(uint16_t);

    // Internal (non-PSRAM) DMA-capable RAM, required for the SPI/DMA
    // transfer used by writePixels() in flush_cb().
    draw_buf_1 = static_cast<uint16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    draw_buf_2 = static_cast<uint16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));

    if (draw_buf_1 == nullptr || draw_buf_2 == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate LVGL draw buffers (%u bytes each)",
                 static_cast<unsigned>(bytes));
        return false;
    }

    return true;
}

} // namespace

bool init()
{
    if (initialized) {
        ESP_LOGW(TAG, "lvgl_port::init() called more than once, ignoring");
        return true;
    }

    if (!display::is_initialized()) {
        ESP_LOGE(TAG, "display::init() must succeed before lvgl_port::init()");
        return false;
    }

    lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    if (lvgl_mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create LVGL mutex");
        return false;
    }

    lv_init();

    const display::Config& disp_cfg = display::config();

    const size_t pixels_per_buffer =
        static_cast<size_t>(disp_cfg.width) * display::params::Lvgl::draw_buffer_lines;

    if (!allocate_draw_buffers(pixels_per_buffer)) {
        vSemaphoreDelete(lvgl_mutex);
        lvgl_mutex = nullptr;
        return false;
    }

    lv_disp = lv_display_create(disp_cfg.width, disp_cfg.height);
    if (lv_disp == nullptr) {
        ESP_LOGE(TAG, "lv_display_create() failed");
        return false;
    }

    lv_display_set_color_format(lv_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(lv_disp, flush_cb);
    lv_display_set_buffers(
        lv_disp,
        draw_buf_1,
        draw_buf_2,
        pixels_per_buffer * sizeof(uint16_t),
        LV_DISPLAY_RENDER_MODE_PARTIAL);

    if (!start_tick_timer()) {
        return false;
    }

    const BaseType_t task_created = xTaskCreate(
        lvgl_task,
        "lvgl",
        display::params::Lvgl::task_stack_size,
        nullptr,
        display::params::Lvgl::task_priority,
        &lvgl_task_handle);

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LVGL task");
        return false;
    }

    initialized = true;
    ESP_LOGI(TAG, "LVGL port initialized (%ux%u, %u-line double buffer)",
             static_cast<unsigned>(disp_cfg.width),
             static_cast<unsigned>(disp_cfg.height),
             static_cast<unsigned>(display::params::Lvgl::draw_buffer_lines));

    return true;
}

bool is_initialized()
{
    return initialized;
}

void lock()
{
    xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);
}

void unlock()
{
    xSemaphoreGiveRecursive(lvgl_mutex);
}

} // namespace lvgl_port
