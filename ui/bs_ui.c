/*
 * Bluffscope Mesh Analyzer - user interface (LVGL 9)
 *
 * Layout (1024 x 600):
 *   +------+---------------------------+-------------+
 *   | rail |          live map         | right panel |
 *   | 72px |           640px           |    312px    |
 *   |      +---------------------------+-------------+
 *   |      |  packet ticker  |  noise / busy / MQTT   |  48px
 *   +------+-----------------------------------------+
 *
 * SPDX-License-Identifier: MIT
 */
#include "bs_ui.h"
#include "bs_model.h"

#include <math.h>
#include <stdio.h>

/* ---------- look ---------- */
#define C_BG      lv_color_hex(0x0E1116)
#define C_RAIL    lv_color_hex(0x151A21)
#define C_PANEL   lv_color_hex(0x171C24)
#define C_CARD    lv_color_hex(0x1E2530)
#define C_BORDER  lv_color_hex(0x262D38)
#define C_GRID    lv_color_hex(0x1A2029)
#define C_TEXT    lv_color_hex(0xE6EAF0)
#define C_QUIET   lv_color_hex(0x8A94A6)
#define C_ACCENT  lv_color_hex(0x4EA8DE)
#define C_GOOD    lv_color_hex(0x3DDC97)
#define C_WARN    lv_color_hex(0xF5B83D)
#define C_CRIT    lv_color_hex(0xF2545B)

#define RAIL_W   72
#define PANEL_W  312
#define STRIP_H  48
#define MAP_W    (BS_SCREEN_W - RAIL_W - PANEL_W)
#define MAP_H    (BS_SCREEN_H - STRIP_H)

static const uint32_t k_pkt_colors[BS_PKT_TYPE_COUNT] = {
    0xB388FF, /* Advert  */
    0x4EA8DE, /* Channel */
    0x3DDC97, /* Direct  */
    0x8A94A6, /* ACK     */
    0xF5B83D, /* Trace   */
    0xFF8A65, /* Path    */
    0x6C7A89, /* Other   */
};

/* ---------- state ---------- */
static lv_obj_t *s_map;
static lv_obj_t *s_sonar;
static lv_obj_t *s_panel_title;
static lv_obj_t *s_cap[6], *s_val[6];   /* 2 x 3 grid of number cards */
static lv_obj_t *s_section;             /* heading under the cards */
static lv_obj_t *s_note;                /* text under the heading */
static lv_obj_t *s_back_btn;
static lv_obj_t *s_ticker;
static lv_obj_t *s_status;
static lv_obj_t *s_mqtt;
static int       s_selected = -1;

/* map projection, rebuilt when repeaters change */
static double s_lat0, s_lon0, s_scale;  /* px per degree of latitude */
static double s_coslat;

/* ---------- helpers ---------- */
static lv_color_t health_color(bs_health_t h)
{
    switch (h) {
    case BS_HEALTH_ACTIVE: return C_GOOD;
    case BS_HEALTH_QUIET:  return C_WARN;
    default:               return C_CRIT;
    }
}

static lv_color_t snr_color(float snr)
{
    if (snr >= 5.0f)  return C_GOOD;
    if (snr >= -5.0f) return C_WARN;
    return C_CRIT;
}

static void compute_projection(void)
{
    double lat_min = 90, lat_max = -90, lon_min = 180, lon_max = -180;
    int n = 0;
    for (int i = 0; i < g_bs.rep_count; i++) {
        const bs_repeater_t *r = &g_bs.rep[i];
        if (!r->has_location) continue;
        if (r->lat < lat_min) lat_min = r->lat;
        if (r->lat > lat_max) lat_max = r->lat;
        if (r->lon < lon_min) lon_min = r->lon;
        if (r->lon > lon_max) lon_max = r->lon;
        n++;
    }
    if (n == 0) { lat_min = 38.2; lat_max = 39.0; lon_min = -121.8; lon_max = -120.9; }

    s_lat0 = (lat_min + lat_max) / 2.0;
    s_lon0 = (lon_min + lon_max) / 2.0;
    s_coslat = cos(s_lat0 * M_PI / 180.0);

    double span_y = (lat_max - lat_min) + 0.0001;
    double span_x = (lon_max - lon_min) * s_coslat + 0.0001;
    double sy = (MAP_H - 120) / span_y;  /* leave room for labels and tray */
    double sx = (MAP_W - 120) / span_x;
    s_scale = sx < sy ? sx : sy;
}

static void project(const lv_area_t *a, double lat, double lon, int32_t *x, int32_t *y)
{
    int32_t cx = (a->x1 + a->x2) / 2;
    int32_t cy = (a->y1 + a->y2) / 2 - 10;
    *x = cx + (int32_t)((lon - s_lon0) * s_coslat * s_scale);
    *y = cy - (int32_t)((lat - s_lat0) * s_scale);
}

static void draw_circle(lv_layer_t *layer, int32_t x, int32_t y, int32_t r,
                        lv_color_t fill, lv_opa_t fill_opa,
                        lv_color_t border, int32_t border_w, lv_opa_t border_opa)
{
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.radius = LV_RADIUS_CIRCLE;
    d.bg_color = fill;
    d.bg_opa = fill_opa;
    d.border_color = border;
    d.border_width = border_w;
    d.border_opa = border_opa;
    lv_area_t a = { x - r, y - r, x + r, y + r };
    lv_draw_rect(layer, &d, &a);
}

static void draw_line(lv_layer_t *layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                      lv_color_t c, int32_t w, lv_opa_t opa, bool dashed)
{
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.p1.x = x1; d.p1.y = y1;
    d.p2.x = x2; d.p2.y = y2;
    d.color = c;
    d.width = w;
    d.opa = opa;
    d.round_start = 1;
    d.round_end = 1;
    if (dashed) { d.dash_width = 6; d.dash_gap = 5; d.round_start = 0; d.round_end = 0; }
    lv_draw_line(layer, &d);
}

static void draw_text(lv_layer_t *layer, const char *txt, int32_t x, int32_t y, int32_t w,
                      lv_color_t c, const lv_font_t *font, lv_text_align_t align)
{
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.text = txt;
    d.color = c;
    d.font = font;
    d.align = align;
    lv_area_t a = { x, y, x + w, y + lv_font_get_line_height(font) };
    lv_draw_label(layer, &d, &a);
}

/* ---------- map ---------- */
static void map_draw_cb(lv_event_t *e)
{
    lv_obj_t   *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t   a;
    lv_obj_get_coords(obj, &a);
    uint32_t now = lv_tick_get();

    /* faint grid so the map reads as a map before tiles are loaded */
    for (int32_t gx = a.x1 + 64; gx < a.x2; gx += 64)
        draw_line(layer, gx, a.y1, gx, a.y2, C_GRID, 1, LV_OPA_COVER, false);
    for (int32_t gy = a.y1 + 64; gy < a.y2; gy += 64)
        draw_line(layer, a.x1, gy, a.x2, gy, C_GRID, 1, LV_OPA_COVER, false);

    /* links: colour and width = median SNR */
    for (int i = 0; i < g_bs.link_count; i++) {
        const bs_link_t *l = &g_bs.link[i];
        const bs_repeater_t *ra = &g_bs.rep[l->a], *rb = &g_bs.rep[l->b];
        if (!ra->has_location || !rb->has_location) continue;
        int32_t x1, y1, x2, y2;
        project(&a, ra->lat, ra->lon, &x1, &y1);
        project(&a, rb->lat, rb->lon, &x2, &y2);
        int32_t w = l->snr_db >= 5 ? 3 : (l->snr_db >= -5 ? 2 : 1);
        draw_line(layer, x1, y1, x2, y2, snr_color(l->snr_db), w, LV_OPA_40, false);
    }

    /* packets travelling hop by hop */
    for (int i = 0; i < BS_MAX_PACKETS; i++) {
        const bs_packet_t *p = &g_bs.pkt[i];
        if (!p->active) continue;
        uint32_t age = now - p->start_ms;
        uint32_t seg = age / p->ms_per_hop;
        if (seg >= (uint32_t)(p->path_len - 1)) continue;   /* landed */
        float f = (float)(age % p->ms_per_hop) / p->ms_per_hop;

        const bs_repeater_t *ra = &g_bs.rep[p->path[seg]];
        const bs_repeater_t *rb = &g_bs.rep[p->path[seg + 1]];
        if (!ra->has_location || !rb->has_location) continue;
        int32_t x1, y1, x2, y2;
        project(&a, ra->lat, ra->lon, &x1, &y1);
        project(&a, rb->lat, rb->lon, &x2, &y2);
        int32_t hx = x1 + (int32_t)((x2 - x1) * f);
        int32_t hy = y1 + (int32_t)((y2 - y1) * f);

        lv_color_t c = lv_color_hex(k_pkt_colors[p->type]);
        bool dashed = (p->ambiguous_hop == seg || p->ambiguous_hop == seg + 1);
        draw_line(layer, x1, y1, hx, hy, c, 3, LV_OPA_80, dashed);
        draw_circle(layer, hx, hy, 5, c, LV_OPA_COVER, c, 0, LV_OPA_TRANSP);
    }

    /* repeaters */
    int no_loc = 0;
    for (int i = 0; i < g_bs.rep_count; i++) {
        const bs_repeater_t *r = &g_bs.rep[i];
        if (!r->has_location) { no_loc++; continue; }
        int32_t x, y;
        project(&a, r->lat, r->lon, &x, &y);

        /* ripple when it just relayed */
        uint32_t since = now - r->last_relay_ms;
        if (r->last_relay_ms && since < 900) {
            int32_t rr = 8 + (int32_t)(since * 22 / 900);
            lv_opa_t o = (lv_opa_t)(LV_OPA_70 - since * LV_OPA_70 / 900);
            draw_circle(layer, x, y, rr, C_BG, LV_OPA_TRANSP, health_color(r->health), 2, o);
        }

        int32_t size = 6 + (int32_t)(r->pkts_hour > 250 ? 5 : r->pkts_hour / 50);
        if (r->is_self) {
            draw_circle(layer, x, y, size + 4, C_BG, LV_OPA_TRANSP, C_ACCENT, 2, LV_OPA_COVER);
            draw_circle(layer, x, y, size - 1, C_ACCENT, LV_OPA_COVER, C_ACCENT, 0, LV_OPA_TRANSP);
        } else {
            draw_circle(layer, x, y, size, health_color(r->health), LV_OPA_COVER,
                        C_BG, 2, LV_OPA_COVER);
        }
        if (i == s_selected)
            draw_circle(layer, x, y, size + 7, C_BG, LV_OPA_TRANSP, C_TEXT, 2, LV_OPA_COVER);
        if (r->advisor_flag)
            draw_circle(layer, x + size, y - size, 4, C_WARN, LV_OPA_COVER, C_BG, 1, LV_OPA_COVER);

        draw_text(layer, r->name, x + size + 6, y - 8, 140,
                  r->is_self ? C_ACCENT : C_QUIET, &lv_font_montserrat_12, LV_TEXT_ALIGN_LEFT);
    }

    /* tray for repeaters with no advertised location: listed, never placed */
    if (no_loc) {
        char buf[48];
        snprintf(buf, sizeof(buf), "No location: %d repeater%s", no_loc, no_loc == 1 ? "" : "s");
        draw_text(layer, buf, a.x1 + 14, a.y2 - 28, 260, C_QUIET,
                  &lv_font_montserrat_12, LV_TEXT_ALIGN_LEFT);
    }

    /* legend */
    int32_t lx = a.x2 - 96, ly = a.y2 - 14 - 16 * BS_PKT_TYPE_COUNT;
    for (int t = 0; t < BS_PKT_TYPE_COUNT; t++) {
        lv_color_t c = lv_color_hex(k_pkt_colors[t]);
        draw_circle(layer, lx, ly + 8 + t * 16, 4, c, LV_OPA_COVER, c, 0, LV_OPA_TRANSP);
        draw_text(layer, bs_pkt_type_name((bs_pkt_type_t)t), lx + 10, ly + t * 16, 80,
                  C_QUIET, &lv_font_montserrat_12, LV_TEXT_ALIGN_LEFT);
    }
}

static void map_click_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);

    int best = -1;
    int32_t best_d2 = 24 * 24;   /* finger-sized hit radius */
    for (int i = 0; i < g_bs.rep_count; i++) {
        const bs_repeater_t *r = &g_bs.rep[i];
        if (!r->has_location) continue;
        int32_t x, y;
        project(&a, r->lat, r->lon, &x, &y);
        int32_t d2 = (x - p.x) * (x - p.x) + (y - p.y) * (y - p.y);
        if (d2 < best_d2) { best_d2 = d2; best = i; }
    }
    bs_ui_select_repeater(best);
}

/* ---------- sonar screensaver ---------- */
static void sonar_draw_cb(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    uint32_t now = lv_tick_get();
    const int32_t cx = BS_SCREEN_W / 2, cy = BS_SCREEN_H / 2 + 10;
    static const int32_t ring[3] = { 85, 170, 255 };
    static const char *ring_label[3] = { "1 hop", "2 hops", "3+ hops" };

    for (int i = 0; i < 3; i++) {
        draw_circle(layer, cx, cy, ring[i], C_BG, LV_OPA_TRANSP, C_ACCENT, 1, LV_OPA_30);
        draw_text(layer, ring_label[i], cx + 6, cy - ring[i] + 4, 80, C_QUIET,
                  &lv_font_montserrat_12, LV_TEXT_ALIGN_LEFT);
    }
    draw_line(layer, cx - ring[2], cy, cx + ring[2], cy, C_ACCENT, 1, LV_OPA_10, false);
    draw_line(layer, cx, cy - ring[2], cx, cy + ring[2], C_ACCENT, 1, LV_OPA_10, false);

    /* sweep: a soft wedge that fades behind a bright edge, one turn every 6 s.
     * LVGL arc angles start at 3 o'clock and run clockwise; bearings start at north. */
    double sweep = fmod(now / 6000.0 * 2.0 * M_PI, 2.0 * M_PI);
    int32_t edge_deg = (int32_t)(sweep * 180.0 / M_PI) - 90;
    static const struct { int32_t span; lv_opa_t opa; } k_tail[] = { {40, 18}, {22, 26}, {10, 40} };
    for (int k = 0; k < 3; k++) {
        lv_draw_arc_dsc_t ad;
        lv_draw_arc_dsc_init(&ad);
        ad.center.x = cx; ad.center.y = cy;
        ad.radius = (uint16_t)ring[2];
        ad.width = ring[2];
        ad.start_angle = (edge_deg - k_tail[k].span + 360) % 360;
        ad.end_angle = (edge_deg + 360) % 360;
        ad.color = C_ACCENT;
        ad.opa = k_tail[k].opa;
        lv_draw_arc(layer, &ad);
    }
    draw_line(layer, cx, cy, cx + (int32_t)(sin(sweep) * ring[2]), cy - (int32_t)(cos(sweep) * ring[2]),
              C_ACCENT, 2, LV_OPA_80, false);

    /* blips: real bearing from us, ring by hop count */
    int self = bs_model_self_index();
    if (self < 0) return;
    const bs_repeater_t *me = &g_bs.rep[self];
    double coslat = cos(me->lat * M_PI / 180.0);

    for (int i = 0; i < g_bs.rep_count; i++) {
        const bs_repeater_t *r = &g_bs.rep[i];
        if (i == self || !r->has_location) continue;
        double dx = (r->lon - me->lon) * coslat, dy = (r->lat - me->lat);
        double bearing = atan2(dx, dy);
        double dist_km = sqrt(dx * dx + dy * dy) * 111.0;
        int hop = r->hops < 1 ? 1 : (r->hops > 3 ? 3 : r->hops);
        int32_t inner = hop == 1 ? 20 : ring[hop - 2];
        double frac = dist_km / 60.0; if (frac > 1) frac = 1;
        int32_t rad = inner + (int32_t)((ring[hop - 1] - inner - 10) * frac);
        int32_t x = cx + (int32_t)(sin(bearing) * rad);
        int32_t y = cy - (int32_t)(cos(bearing) * rad);

        /* fade with silence: bright if it relayed in the last 30 s */
        uint32_t since = now - r->last_relay_ms;
        lv_opa_t opa = since < 30000 ? (lv_opa_t)(LV_OPA_COVER - since * 150 / 30000) : LV_OPA_40;
        if (r->health == BS_HEALTH_SILENT) opa = LV_OPA_20;
        lv_color_t c = r->hops == 1 ? snr_color(r->snr_db) : C_ACCENT;

        if (r->last_relay_ms && since < 1200) {
            int32_t rr = 6 + (int32_t)(since * 26 / 1200);
            draw_circle(layer, x, y, rr, C_BG, LV_OPA_TRANSP, c, 2,
                        (lv_opa_t)(LV_OPA_80 - since * LV_OPA_80 / 1200));
        }
        draw_circle(layer, x, y, 5, c, opa, c, 0, LV_OPA_TRANSP);
        draw_text(layer, r->name, x + 9, y - 7, 170, C_QUIET, &lv_font_montserrat_12,
                  LV_TEXT_ALIGN_LEFT);
    }
    draw_circle(layer, cx, cy, 6, C_ACCENT, LV_OPA_COVER, C_ACCENT, 0, LV_OPA_TRANSP);
}

static void sonar_click_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    bs_ui_show_sonar(false);
}

void bs_ui_show_sonar(bool on)
{
    if (on) lv_obj_remove_flag(s_sonar, LV_OBJ_FLAG_HIDDEN);
    else    lv_obj_add_flag(s_sonar, LV_OBJ_FLAG_HIDDEN);
}

/* ---------- right panel ---------- */
static void back_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    bs_ui_select_repeater(-1);
}

static void set_card(int i, const char *cap, const char *val, lv_color_t c)
{
    lv_label_set_text(s_cap[i], cap);
    lv_label_set_text(s_val[i], val);
    lv_obj_set_style_text_color(s_val[i], c, 0);
}

/* Top five repeaters by score, for the overview */
static void top_repeaters_text(char *out, size_t len)
{
    int order[BS_MAX_REPEATERS], n = 0;
    for (int i = 0; i < g_bs.rep_count; i++)   /* silent repeaters can't rank */
        if (!g_bs.rep[i].is_self && g_bs.rep[i].health != BS_HEALTH_SILENT) order[n++] = i;
    for (int i = 1; i < n; i++) {               /* insertion sort, n is small */
        int k = order[i], j = i - 1;
        while (j >= 0 && g_bs.rep[order[j]].score < g_bs.rep[k].score) { order[j + 1] = order[j]; j--; }
        order[j + 1] = k;
    }
    size_t pos = 0;
    out[0] = '\0';
    for (int i = 0; i < n && i < 5 && pos < len; i++) {
        const bs_repeater_t *r = &g_bs.rep[order[i]];
        pos += snprintf(out + pos, len - pos, "%3.0f    %s\n", (double)r->score, r->name);
    }
}

static void refresh_panel(void)
{
    char v[6][32];
    char note[400];

    if (s_selected < 0 || s_selected >= g_bs.rep_count) {
        lv_label_set_text(s_panel_title, "SMF overview");
        int heard = 0, silent = 0, flagged = 0;
        for (int i = 0; i < g_bs.rep_count; i++) {
            if (g_bs.rep[i].is_self) continue;
            if (g_bs.rep[i].health == BS_HEALTH_SILENT) silent++; else heard++;
            if (g_bs.rep[i].advisor_flag) flagged++;
        }
        snprintf(v[0], 32, "%lu", (unsigned long)g_bs.pkts_hour);
        snprintf(v[1], 32, "%d  (%d silent)", heard, silent);
        snprintf(v[2], 32, "%.0f dBm", (double)g_bs.noise_floor_dbm);
        snprintf(v[3], 32, "%.0f %%", (double)g_bs.channel_busy_pct);
        snprintf(v[4], 32, "%s", g_bs.mqtt_connected ? "Connected" : "Offline");
        snprintf(v[5], 32, "%d waiting", flagged);
        set_card(0, "Packets / hour", v[0], C_TEXT);
        set_card(1, "Repeaters heard", v[1], C_TEXT);
        set_card(2, "Noise floor", v[2], C_TEXT);
        set_card(3, "Channel busy", v[3], C_TEXT);
        set_card(4, "Observer uplink", v[4], g_bs.mqtt_connected ? C_GOOD : C_CRIT);
        set_card(5, "Advisor", v[5], flagged ? C_WARN : C_TEXT);

        lv_label_set_text(s_section, "Top repeaters  (7-day score)");
        lv_obj_set_style_text_color(s_section, C_TEXT, 0);
        top_repeaters_text(note, sizeof(note));
        lv_label_set_text(s_note, note);
        lv_obj_add_flag(s_back_btn, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    const bs_repeater_t *r = &g_bs.rep[s_selected];
    static const char *health_txt[] = { "Active", "Quiet", "Silent" };
    lv_label_set_text(s_panel_title, r->name);
    snprintf(v[0], 32, "%04X", (unsigned)r->prefix);
    snprintf(v[2], 32, "%u", (unsigned)r->hops);
    if (r->hops == 1) snprintf(v[3], 32, "%.1f dB", (double)r->snr_db);
    else              snprintf(v[3], 32, "Not direct");
    snprintf(v[4], 32, "%lu", (unsigned long)r->pkts_hour);
    snprintf(v[5], 32, "%.0f / 100", (double)r->score);
    set_card(0, "Prefix", v[0], C_TEXT);
    set_card(1, "Health", health_txt[r->health], health_color(r->health));
    set_card(2, "Hops from here", v[2], C_TEXT);
    set_card(3, "Direct SNR", v[3], r->hops == 1 ? snr_color(r->snr_db) : C_QUIET);
    set_card(4, "Relayed / hour", v[4], C_TEXT);
    set_card(5, "Score", v[5], C_TEXT);

    if (r->advisor_flag) {
        lv_label_set_text(s_section, "Advisor suggestion  (demo)");
        lv_obj_set_style_text_color(s_section, C_WARN, 0);
        snprintf(note, sizeof(note),
                 "set txdelay 1.4     (now 0.5)\n\n"
                 "Why: 9 neighbours, 18%% duplicate\n"
                 "rebroadcasts, 7 days observed.\n"
                 "Confidence: medium.\n\n"
                 "The owner applies it.\n"
                 "Bluffscope never changes a repeater.");
    } else {
        lv_label_set_text(s_section, "Advisor");
        lv_obj_set_style_text_color(s_section, C_TEXT, 0);
        snprintf(note, sizeof(note), "No suggestion. Timing looks right\nfor this repeater's traffic.");
    }
    lv_label_set_text(s_note, note);
    lv_obj_remove_flag(s_back_btn, LV_OBJ_FLAG_HIDDEN);
}

void bs_ui_select_repeater(int index)
{
    s_selected = index;
    refresh_panel();
    lv_obj_invalidate(s_map);
}

/* ---------- bottom strip ---------- */
static void refresh_strip(void)
{
    char buf[160];
    snprintf(buf, sizeof(buf), "%s", g_bs.ticker[0][0] ? g_bs.ticker[0] : "Listening on SMF...");
    lv_label_set_text(s_ticker, buf);
    snprintf(buf, sizeof(buf), "Noise %.0f dBm    Busy %.0f%%    MQTT",
             (double)g_bs.noise_floor_dbm, (double)g_bs.channel_busy_pct);
    lv_label_set_text(s_status, buf);
    lv_label_set_text(s_mqtt, g_bs.mqtt_connected ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(s_mqtt, g_bs.mqtt_connected ? C_GOOD : C_CRIT, 0);
    lv_obj_align_to(s_status, s_mqtt, LV_ALIGN_OUT_LEFT_MID, -8, 0);
}

/* ---------- rail ---------- */
static void rail_cb(lv_event_t *e)
{
    const char *what = lv_event_get_user_data(e);
    if (what && what[0] == 'S' && what[1] == 'o') bs_ui_show_sonar(true);
    /* Other pages (Repeaters, Advisor, Packets, Health, Settings) come in
     * later phases; they open in the right panel so the map stays visible. */
}

static void add_rail_button(lv_obj_t *rail, const char *symbol, const char *name, int row)
{
    lv_obj_t *b = lv_button_create(rail);
    lv_obj_set_size(b, 56, 56);
    lv_obj_set_pos(b, 8, 12 + row * 68);
    lv_obj_set_style_bg_color(b, row == 0 ? C_ACCENT : C_CARD, 0);
    lv_obj_set_style_bg_opa(b, row == 0 ? LV_OPA_30 : LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_add_event_cb(b, rail_cb, LV_EVENT_CLICKED, (void *)name);

    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, symbol);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(l, row == 0 ? C_ACCENT : C_TEXT, 0);
    lv_obj_center(l);
}

/* ---------- refresh loop ---------- */
static void ui_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    static uint32_t last_text = 0;
    uint32_t now = lv_tick_get();
    bs_model_tick(now);

    if (lv_obj_has_flag(s_sonar, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_invalidate(s_map);
        if (lv_display_get_inactive_time(NULL) > BS_SCREENSAVER_S * 1000U)
            bs_ui_show_sonar(true);
    } else {
        lv_obj_invalidate(s_sonar);
    }

    if (now - last_text > 500) {
        last_text = now;
        refresh_panel();
        refresh_strip();
    }
}

static lv_obj_t *plain_box(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, lv_color_t bg)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, bg, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

void bs_ui_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    compute_projection();

    /* left rail */
    lv_obj_t *rail = plain_box(scr, 0, 0, RAIL_W, BS_SCREEN_H, C_RAIL);
    add_rail_button(rail, LV_SYMBOL_GPS,      "Map",       0);
    add_rail_button(rail, LV_SYMBOL_LIST,     "Repeaters", 1);
    add_rail_button(rail, LV_SYMBOL_EDIT,     "Advisor",   2);
    add_rail_button(rail, LV_SYMBOL_SHUFFLE,  "Packets",   3);
    add_rail_button(rail, LV_SYMBOL_WARNING,  "Health",    4);
    add_rail_button(rail, LV_SYMBOL_EYE_OPEN, "Sonar",     5);
    add_rail_button(rail, LV_SYMBOL_SETTINGS, "Settings",  7);

    /* map */
    s_map = plain_box(scr, RAIL_W, 0, MAP_W, MAP_H, C_BG);
    lv_obj_add_flag(s_map, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_map, map_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(s_map, map_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *brand = lv_label_create(s_map);
    lv_label_set_text(brand, "BLUFFSCOPE");
    lv_obj_set_style_text_font(brand, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(brand, C_TEXT, 0);
    lv_obj_set_style_text_letter_space(brand, 3, 0);
    lv_obj_set_pos(brand, 16, 12);
    lv_obj_t *sub = lv_label_create(s_map);
    lv_label_set_text(sub, "SMF  909.875 MHz  BW62.5  SF9  CR5");
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sub, C_QUIET, 0);
    lv_obj_set_pos(sub, 16, 34);

    /* right panel */
    lv_obj_t *panel = plain_box(scr, RAIL_W + MAP_W, 0, PANEL_W, MAP_H, C_PANEL);
    lv_obj_set_style_border_side(panel, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, C_BORDER, 0);
    s_panel_title = lv_label_create(panel);
    lv_obj_set_style_text_font(s_panel_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_panel_title, C_TEXT, 0);
    lv_obj_set_pos(s_panel_title, 20, 18);
    lv_label_set_long_mode(s_panel_title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_panel_title, PANEL_W - 40);

    for (int i = 0; i < 6; i++) {
        lv_obj_t *card = plain_box(panel, 20 + (i % 2) * 140, 58 + (i / 2) * 68, 132, 60, C_CARD);
        lv_obj_set_style_radius(card, 10, 0);
        s_cap[i] = lv_label_create(card);
        lv_obj_set_style_text_font(s_cap[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(s_cap[i], C_QUIET, 0);
        lv_obj_set_pos(s_cap[i], 10, 8);
        s_val[i] = lv_label_create(card);
        lv_obj_set_style_text_font(s_val[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(s_val[i], C_TEXT, 0);
        lv_label_set_long_mode(s_val[i], LV_LABEL_LONG_DOT);
        lv_obj_set_width(s_val[i], 116);
        lv_obj_set_pos(s_val[i], 10, 30);
    }
    s_section = lv_label_create(panel);
    lv_obj_set_style_text_font(s_section, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(s_section, 20, 274);
    s_note = lv_label_create(panel);
    lv_obj_set_style_text_font(s_note, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_note, C_QUIET, 0);
    lv_obj_set_style_text_line_space(s_note, 4, 0);
    lv_obj_set_width(s_note, PANEL_W - 40);
    lv_obj_set_pos(s_note, 20, 302);

    s_back_btn = lv_button_create(panel);
    lv_obj_set_size(s_back_btn, 120, 44);
    lv_obj_align(s_back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -16);
    lv_obj_set_style_bg_color(s_back_btn, C_CARD, 0);
    lv_obj_set_style_shadow_width(s_back_btn, 0, 0);
    lv_obj_add_event_cb(s_back_btn, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bl = lv_label_create(s_back_btn);
    lv_label_set_text(bl, LV_SYMBOL_LEFT "  Overview");
    lv_obj_center(bl);

    /* bottom strip */
    lv_obj_t *strip = plain_box(scr, RAIL_W, MAP_H, BS_SCREEN_W - RAIL_W, STRIP_H, C_RAIL);
    s_ticker = lv_label_create(strip);
    lv_obj_set_style_text_font(s_ticker, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_ticker, C_TEXT, 0);
    lv_obj_align(s_ticker, LV_ALIGN_LEFT_MID, 16, 0);
    s_mqtt = lv_label_create(strip);
    lv_obj_set_style_text_font(s_mqtt, &lv_font_montserrat_14, 0);
    lv_obj_align(s_mqtt, LV_ALIGN_RIGHT_MID, -16, 0);
    s_status = lv_label_create(strip);
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status, C_QUIET, 0);

    /* sonar screensaver, full screen, hidden until idle */
    s_sonar = plain_box(scr, 0, 0, BS_SCREEN_W, BS_SCREEN_H, lv_color_hex(0x05070A));
    lv_obj_add_flag(s_sonar, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_sonar, sonar_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(s_sonar, sonar_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *st = lv_label_create(s_sonar);
    lv_label_set_text(st, "BLUFFSCOPE  SONAR");
    lv_obj_set_style_text_font(st, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(st, C_ACCENT, 0);
    lv_obj_set_style_text_letter_space(st, 3, 0);
    lv_obj_set_pos(st, 24, 20);
    lv_obj_t *hint = lv_label_create(s_sonar);
    lv_label_set_text(hint, "Touch to return to the map");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, C_QUIET, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -16);

    refresh_panel();
    refresh_strip();
    lv_timer_create(ui_timer_cb, 33, NULL);
}
