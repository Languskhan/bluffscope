# Hoptics devices

Every device runs the same idea: subscribe to the plugin's feed, show it at the size the screen
can handle. Devices with their own LoRa radio are also full MeshCore nodes.

Pin numbers are never guessed. Each board's pins come from the maker's own examples for that
exact board, and are written down with the link they came from.

## Tiers

| Tier | What it shows | Devices |
| --- | --- | --- |
| **Full** | The whole Map screen: map, alerts, side panels, channel strip | CrowPanel 10.1", Cardputer Zero on HDMI |
| **Field** | Handheld pages: map, alerts with Send, feed, balloon chase, health | T-Deck Plus, T-Deck |
| **Compact** | Multi-page: map page, alerts, mesh health, feed, balloon | Heltec V4 and V4-R8 v2 expansion kits |
| **Lite** | Text pages flipped with keys: alerts, health, feed, balloon | M5Stack Cardputer, Cardputer Zero's own screen |
| **Ticker** | One rotating line or two: newest alert, mesh health, balloon | Heltec V3 (128x64 OLED) |

## Device list

| Device | Chip | Data | Notes | Status |
| --- | --- | --- | --- | --- |
| Elecrow CrowPanel Advanced 10.1" | ESP32-P4 + ESP32-C6 (Wi-Fi) + SX1262 | Feed + own radio | Main target, LVGL, touch | Simulator working; board bring-up next |
| Cardputer Zero (HDMI) | Linux | Feed (web) | The plugin's web dashboard full-screen in a kiosk browser; no separate app | Comes with the plugin |
| Cardputer Zero (own screen) | Linux | Feed | Terminal-style pages | Planned |
| LilyGo T-Deck Plus | ESP32-S3 + SX1262 + GPS | Feed + own radio | 320x240, keyboard, trackball; chase mode uses its GPS | Planned |
| LilyGo T-Deck | ESP32-S3 + SX1262 | Feed + own radio | Same build as the Plus without GPS features | Planned |
| Heltec V4 expansion kit | ESP32-S3 + SX1262 | Feed + own radio | Pages sized to the kit's screen | Planned |
| Heltec V4-R8 v2 expansion kit | ESP32-S3 + SX1262 | Feed + own radio | Same build as V4, different board file | Planned |
| M5Stack Cardputer | ESP32-S3 | Feed (Wi-Fi) | 240x135, keyboard to flip pages; no LoRa | Planned |
| Heltec V3 | ESP32-S3 + SX1262 | Feed + own radio | Ticker only; the OLED is too small for more | Optional |

## Devices that don't need Hoptics firmware

- **nRF52 boards without Wi-Fi** (Heltec T114, Wio Tracker L1 Pro, and the L2 Pro if it is the
  same family) can't receive the feed. Keep them on normal MeshCore firmware and join `#alerts`:
  they still get every alert that is sent.
- **Boards without a screen** (Station G2/G3, ThinkNode M6/M7, T1000) stay repeaters, observers
  or trackers.

## Candidate devices (not committed)

Any ESP32 board with Wi-Fi and a screen can be a Hoptics display. Boards with an SX1262 can also
be mesh nodes. Good fits to look at:

- **LilyGo P4 display:** ESP32-P4 class, a second full-dashboard screen
- **M5Stack Tab5:** ESP32-P4 with a 5" screen, a portable full dashboard
- **Seeed SenseCAP Indicator (LoRa version):** 4" touch screen with LoRa built in, a desk node
- **LilyGo T-Deck Plus:** screen, keyboard, LoRa and GPS, a field version
- **LilyGo T-LoRa Pager:** ESP32-S3 pager with a 2.33" screen and LoRa
- **Heltec Vision Master (e-paper):** very low power, good for an always-on alerts and health board

Check each board's current specs before buying; this list is ideas, not a promise.

## Adding a new device

1. Add `boards/<device>/board.h` with pins copied from the maker's example (link in the header).
2. Pick a tier and a screen layout (`ui/` is shared; tiers choose which screens to build).
3. Run the security checklist in `docs/DESIGN.md` before the first release build.
