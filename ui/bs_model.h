/*
 * Bluffscope Mesh Analyzer - shared data model
 *
 * Everything the screens draw comes from here. On the real board the
 * MeshCore radio code fills it; in the PC simulator, sim/fake_mesh.c does.
 * The UI never talks to the radio directly.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef BS_MODEL_H
#define BS_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#define BS_MAX_REPEATERS  48
#define BS_MAX_LINKS      96
#define BS_MAX_PACKETS    24   /* packets animating on the map at once */
#define BS_MAX_PATH       8    /* hops drawn per packet */
#define BS_TICKER_LINES   4

/* MeshCore packet types we colour on the map */
typedef enum {
    BS_PKT_ADVERT = 0,
    BS_PKT_CHANNEL,
    BS_PKT_DIRECT,
    BS_PKT_ACK,
    BS_PKT_TRACE,
    BS_PKT_PATH,
    BS_PKT_OTHER,
    BS_PKT_TYPE_COUNT
} bs_pkt_type_t;

typedef enum {
    BS_HEALTH_ACTIVE = 0,  /* heard recently, relaying */
    BS_HEALTH_QUIET,       /* heard, but less than usual */
    BS_HEALTH_SILENT       /* missed 3x its usual advert interval */
} bs_health_t;

typedef struct {
    char     name[24];
    uint16_t prefix;        /* 2-byte path hash, as SMF uses */
    bool     has_location;
    double   lat, lon;
    bool     is_self;       /* this Bluffscope node */
    bs_health_t health;
    uint8_t  hops;          /* hops from us (1 = heard directly) */
    float    snr_db;        /* last direct SNR, if heard directly */
    uint32_t pkts_hour;     /* relayed packets in the last hour */
    uint32_t last_relay_ms; /* drives the ripple effect */
    float    score;         /* 0..100 ranking score */
    bool     advisor_flag;  /* Advisor has a suggestion */
} bs_repeater_t;

typedef struct {
    uint8_t a, b;           /* repeater indexes */
    float   snr_db;         /* median link SNR */
} bs_link_t;

typedef struct {
    bool          active;
    bs_pkt_type_t type;
    uint8_t       path[BS_MAX_PATH];
    uint8_t       path_len;
    uint8_t       ambiguous_hop; /* 0xFF = none; else hop index whose prefix collides */
    uint32_t      start_ms;
    uint16_t      ms_per_hop;
} bs_packet_t;

typedef struct {
    bs_repeater_t rep[BS_MAX_REPEATERS];
    uint8_t       rep_count;
    bs_link_t     link[BS_MAX_LINKS];
    uint8_t       link_count;
    bs_packet_t   pkt[BS_MAX_PACKETS];

    /* overview numbers for the right panel and bottom strip */
    uint32_t pkts_hour;
    float    noise_floor_dbm;
    float    channel_busy_pct;
    bool     mqtt_connected;
    char     ticker[BS_TICKER_LINES][64];
} bs_model_t;

extern bs_model_t g_bs;

void        bs_model_init(void);
int         bs_model_add_repeater(const bs_repeater_t *r);
void        bs_model_add_link(uint8_t a, uint8_t b, float snr_db);
void        bs_model_add_packet(bs_pkt_type_t type, const uint8_t *path,
                                uint8_t path_len, uint32_t now_ms);
void        bs_model_tick(uint32_t now_ms);
int         bs_model_self_index(void);
const char *bs_pkt_type_name(bs_pkt_type_t t);

#endif
