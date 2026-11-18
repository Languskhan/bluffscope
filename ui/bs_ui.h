/*
 * Bluffscope Mesh Analyzer - user interface (LVGL 9)
 *
 * The same code runs on the CrowPanel 10.1" and in the PC simulator.
 * Screen: 1024 x 600 landscape.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef BS_UI_H
#define BS_UI_H

#include "lvgl.h"
#include <stdbool.h>

#define BS_SCREEN_W 1024
#define BS_SCREEN_H 600

/* Seconds without a touch before the sonar screensaver starts. */
#define BS_SCREENSAVER_S 120

void bs_ui_create(void);
void bs_ui_select_repeater(int index);   /* -1 = back to overview */
void bs_ui_show_sonar(bool on);

#endif
