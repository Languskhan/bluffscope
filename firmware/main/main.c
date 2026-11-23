/*
 * Hoptics - CrowPanel Advanced 10.1" (ESP32-P4) firmware
 *
 * PHASE 2 SKELETON. This file shows where each piece will plug in; it is
 * not expected to build until the board arrives and the display, touch and
 * SX1262 drivers from Elecrow's examples are brought in (see board.h).
 *
 * SPDX-License-Identifier: MIT
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lvgl.h"

#include "board.h"
#include "bs_model.h"
#include "bs_ui.h"

static const char *TAG = "hoptics";

void app_main(void)
{
    ESP_LOGI(TAG, "Hoptics starting");

    /* Phase 2: bring-up
     *  - board_display_init(): MIPI-DSI panel, backlight
     *  - board_touch_init():   GT911 over I2C
     *  - board_sdcard_init():  microSD for tiles and history
     *  - board_lora_init():    SX1262 on SPI, receive on 909.875 MHz
     */
    board_init();

    lv_init();
    board_lvgl_attach();          /* display + touch drivers into LVGL */

    bs_model_init();
    bs_ui_create();

    /* Phase 3: MeshCore stack as an ESP-IDF component; decoded packets
     * call bs_model_add_packet(). Phase 4: observer uplink over the C6. */

    for (;;) {
        uint32_t wait_ms = lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(wait_ms < 5 ? 5 : wait_ms));
    }
}
