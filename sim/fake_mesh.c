/*
 * Bluffscope simulator - fake SMF-style traffic
 *
 * Every node here is invented ("DEMO ...") so screenshots never show
 * real repeaters or real owners. Positions are only roughly spread over
 * the Sacramento valley so the map looks like the real thing.
 *
 * SPDX-License-Identifier: MIT
 */
#include "fake_mesh.h"
#include "bs_model.h"

#include <stdlib.h>
#include <string.h>

typedef struct { const char *name; uint16_t prefix; double lat, lon; bool loc; } demo_node_t;

static const demo_node_t k_nodes[] = {
    { "BLUFFSCOPE (you)",   0xB1F5, 38.64, -121.27, true  },
    { "DEMO North Ridge",   0x1A2C, 38.92, -121.08, true  },
    { "DEMO Foothill Peak", 0x2B7E, 38.75, -120.86, true  },
    { "DEMO Valley Tower",  0x3C01, 38.58, -121.49, true  },
    { "DEMO Delta East",    0x4D9A, 38.32, -121.43, true  },
    { "DEMO River Bend",    0x5E33, 38.68, -121.62, true  },
    { "DEMO Lake View",     0x6F48, 38.72, -121.13, true  },
    { "DEMO South Hill",    0x70B2, 38.41, -121.12, true  },
    { "DEMO Airport West",  0x81C4, 38.79, -121.58, true  },
    { "DEMO Ridge Twin",    0x1A2C, 39.05, -121.30, true  },  /* same prefix as North Ridge on purpose */
    { "DEMO Canyon Relay",  0x92D6, 38.86, -120.95, true  },
    { "DEMO Rooftop",       0xA3E7, 0, 0,           false },  /* no advertised location */
};
#define N_NODES ((int)(sizeof(k_nodes) / sizeof(k_nodes[0])))

static const struct { uint8_t a, b; float snr; } k_links[] = {
    {0, 6, 8.5f}, {0, 3, 6.0f}, {0, 7, -2.5f}, {6, 2, 4.0f}, {6, 1, 1.5f},
    {1, 10, 7.0f}, {2, 10, 9.0f}, {3, 5, 3.0f}, {3, 4, -7.0f}, {5, 8, 10.5f},
    {3, 8, 2.0f}, {7, 4, -3.5f}, {1, 9, -1.0f}, {7, 2, -6.5f}, {0, 11, 5.0f},
};
#define N_LINKS ((int)(sizeof(k_links) / sizeof(k_links[0])))

static uint32_t s_next_ms;

static int neighbours(int n, int *out)
{
    int c = 0;
    for (int i = 0; i < N_LINKS; i++) {
        if (k_links[i].a == n) out[c++] = k_links[i].b;
        else if (k_links[i].b == n) out[c++] = k_links[i].a;
    }
    return c;
}

static void compute_hops(void)
{
    int dist[N_NODES];
    for (int i = 0; i < N_NODES; i++) dist[i] = 99;
    int queue[N_NODES], qh = 0, qt = 0;
    dist[0] = 0; queue[qt++] = 0;
    while (qh < qt) {
        int n = queue[qh++], nb[N_NODES];
        int c = neighbours(n, nb);
        for (int i = 0; i < c; i++)
            if (dist[nb[i]] > dist[n] + 1) { dist[nb[i]] = dist[n] + 1; queue[qt++] = nb[i]; }
    }
    for (int i = 0; i < N_NODES; i++) g_bs.rep[i].hops = (uint8_t)dist[i];
}

void fake_mesh_init(void)
{
    srand(42);  /* same demo every run */
    for (int i = 0; i < N_NODES; i++) {
        bs_repeater_t r = {0};
        strncpy(r.name, k_nodes[i].name, sizeof(r.name) - 1);
        r.prefix = k_nodes[i].prefix;
        r.lat = k_nodes[i].lat;
        r.lon = k_nodes[i].lon;
        r.has_location = k_nodes[i].loc;
        r.is_self = (i == 0);
        r.health = BS_HEALTH_ACTIVE;
        r.pkts_hour = 20 + rand() % 260;
        r.score = 40 + rand() % 55;
        r.snr_db = -8.0f + (rand() % 190) / 10.0f;
        bs_model_add_repeater(&r);
    }
    g_bs.rep[7].health = BS_HEALTH_QUIET;
    g_bs.rep[4].health = BS_HEALTH_SILENT;
    g_bs.rep[4].pkts_hour = 0;
    g_bs.rep[3].advisor_flag = true;
    g_bs.rep[8].advisor_flag = true;

    for (int i = 0; i < N_LINKS; i++)
        bs_model_add_link(k_links[i].a, k_links[i].b, k_links[i].snr);
    compute_hops();

    g_bs.pkts_hour = 412;
    g_bs.noise_floor_dbm = -108.0f;
    g_bs.channel_busy_pct = 12.0f;
    g_bs.mqtt_connected = true;
    s_next_ms = 0;
}

void fake_mesh_step(uint32_t now_ms)
{
    if (now_ms < s_next_ms) return;
    s_next_ms = now_ms + 350 + rand() % 700;

    /* random walk through the link graph, never revisiting a node */
    uint8_t path[BS_MAX_PATH];
    int len = 0, cur;
    do { cur = rand() % N_NODES; } while (g_bs.rep[cur].health == BS_HEALTH_SILENT);
    int target_len = 2 + rand() % 4;
    path[len++] = (uint8_t)cur;
    while (len < target_len) {
        int nb[N_NODES], c = neighbours(cur, nb), pick = -1;
        for (int tries = 0; tries < 6 && c > 0; tries++) {
            int cand = nb[rand() % c];
            bool seen = false;
            for (int k = 0; k < len; k++) if (path[k] == cand) seen = true;
            if (!seen && g_bs.rep[cand].health != BS_HEALTH_SILENT) { pick = cand; break; }
        }
        if (pick < 0) break;
        path[len++] = (uint8_t)pick;
        cur = pick;
    }
    if (len < 2) return;

    static const bs_pkt_type_t mix[] = {
        BS_PKT_CHANNEL, BS_PKT_CHANNEL, BS_PKT_CHANNEL, BS_PKT_ADVERT, BS_PKT_ADVERT,
        BS_PKT_DIRECT, BS_PKT_DIRECT, BS_PKT_ACK, BS_PKT_PATH, BS_PKT_TRACE, BS_PKT_OTHER
    };
    bs_model_add_packet(mix[rand() % (int)(sizeof(mix) / sizeof(mix[0]))], path, (uint8_t)len, now_ms);

    /* drift the noise floor and channel load a little */
    g_bs.noise_floor_dbm = -110.0f + (rand() % 50) / 10.0f;
    g_bs.channel_busy_pct = 8.0f + (rand() % 90) / 10.0f;
}
