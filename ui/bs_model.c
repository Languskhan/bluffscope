/*
 * Bluffscope Mesh Analyzer - shared data model
 * SPDX-License-Identifier: MIT
 */
#include "bs_model.h"

#include <stdio.h>
#include <string.h>

bs_model_t g_bs;

static const char *k_type_names[BS_PKT_TYPE_COUNT] = {
    "Advert", "Channel", "Direct", "ACK", "Trace", "Path", "Other"
};

const char *bs_pkt_type_name(bs_pkt_type_t t)
{
    return (t < BS_PKT_TYPE_COUNT) ? k_type_names[t] : "?";
}

void bs_model_init(void)
{
    memset(&g_bs, 0, sizeof(g_bs));
    g_bs.noise_floor_dbm = -110.0f;
}

int bs_model_add_repeater(const bs_repeater_t *r)
{
    if (g_bs.rep_count >= BS_MAX_REPEATERS) return -1;
    g_bs.rep[g_bs.rep_count] = *r;
    return g_bs.rep_count++;
}

void bs_model_add_link(uint8_t a, uint8_t b, float snr_db)
{
    if (g_bs.link_count >= BS_MAX_LINKS) return;
    if (a >= g_bs.rep_count || b >= g_bs.rep_count) return;
    g_bs.link[g_bs.link_count++] = (bs_link_t){ .a = a, .b = b, .snr_db = snr_db };
}

int bs_model_self_index(void)
{
    for (int i = 0; i < g_bs.rep_count; i++)
        if (g_bs.rep[i].is_self) return i;
    return -1;
}

/* Two repeaters sharing a 2-byte prefix means a path hop can't be pinned
 * to one of them. The map draws such hops dashed instead of guessing. */
static bool prefix_is_ambiguous(uint16_t prefix)
{
    int n = 0;
    for (int i = 0; i < g_bs.rep_count; i++)
        if (g_bs.rep[i].prefix == prefix) n++;
    return n > 1;
}

static void ticker_push(const char *line)
{
    for (int i = BS_TICKER_LINES - 1; i > 0; i--)
        memcpy(g_bs.ticker[i], g_bs.ticker[i - 1], sizeof(g_bs.ticker[0]));
    snprintf(g_bs.ticker[0], sizeof(g_bs.ticker[0]), "%s", line);
}

void bs_model_add_packet(bs_pkt_type_t type, const uint8_t *path,
                         uint8_t path_len, uint32_t now_ms)
{
    if (path_len < 2) return;
    if (path_len > BS_MAX_PATH) path_len = BS_MAX_PATH;

    bs_packet_t *slot = NULL;
    for (int i = 0; i < BS_MAX_PACKETS; i++) {
        if (!g_bs.pkt[i].active) { slot = &g_bs.pkt[i]; break; }
    }
    if (!slot) return; /* map is busy; the packet still counts below */

    memset(slot, 0, sizeof(*slot));
    slot->active = true;
    slot->type = type;
    slot->path_len = path_len;
    slot->start_ms = now_ms;
    slot->ms_per_hop = 700;
    slot->ambiguous_hop = 0xFF;
    for (uint8_t i = 0; i < path_len; i++) {
        uint8_t idx = path[i] < g_bs.rep_count ? path[i] : 0;
        slot->path[i] = idx;
        if (slot->ambiguous_hop == 0xFF && prefix_is_ambiguous(g_bs.rep[idx].prefix))
            slot->ambiguous_hop = i;
    }

    char line[64];
    const bs_repeater_t *last = &g_bs.rep[slot->path[path_len - 1]];
    snprintf(line, sizeof(line), "%-7s %u hops  last %04X %s",
             bs_pkt_type_name(type), (unsigned)(path_len - 1),
             (unsigned)last->prefix, last->name);
    ticker_push(line);
}

void bs_model_tick(uint32_t now_ms)
{
    for (int i = 0; i < BS_MAX_PACKETS; i++) {
        bs_packet_t *p = &g_bs.pkt[i];
        if (!p->active) continue;

        uint32_t age = now_ms - p->start_ms;
        uint32_t hop = age / p->ms_per_hop;      /* hop the packet has reached */
        if (hop < p->path_len)
            g_bs.rep[p->path[hop]].last_relay_ms = p->start_ms + hop * p->ms_per_hop;

        /* keep it on screen a moment after it lands, then free the slot */
        if (age > (uint32_t)p->path_len * p->ms_per_hop + 600)
            p->active = false;
    }
}
