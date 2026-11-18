/*
 * Board support: Elecrow CrowPanel Advanced 10.1" (DHE04310D), ESP32-P4.
 *
 * Pin numbers are deliberately NOT filled in yet. Copy them from Elecrow's
 * own examples for this exact board so nothing is guessed:
 *   - SX1262: "Lesson 14 SX1262 LoRa" (ESP-IDF), wireless module slot pins
 *   - Display (MIPI-DSI) and GT911 touch: the factory LVGL example
 * https://www.elecrow.com/wiki/P4_IDF_14_SX1262_LoRa.html
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef BOARD_H
#define BOARD_H

/* SMF radio preset. Fixed by design: Bluffscope is SMF-only. */
#define BS_LORA_FREQ_MHZ   909.875f
#define BS_LORA_BW_KHZ     62.5f
#define BS_LORA_SF         9
#define BS_LORA_CR         5

void board_init(void);
void board_lvgl_attach(void);

#endif
