/* Bluffscope simulator - fake SMF-style traffic. SPDX-License-Identifier: MIT */
#ifndef FAKE_MESH_H
#define FAKE_MESH_H
#include <stdint.h>

void fake_mesh_init(void);
void fake_mesh_step(uint32_t now_ms);

#endif
