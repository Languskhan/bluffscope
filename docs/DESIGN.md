# Hoptics design notes

- Original spec (Bluffscope era): https://claude.ai/artifact/1uUsMYUS5tBJD8BzwkpKUm
- Hoptics screen design and logo options: https://claude.ai/artifact/SKwLCCkbmShPaVzNCZ5Y1T

## Rules that never bend

1. SMF only: 909.875 MHz, BW 62.5 kHz, SF9, CR5. Not a setting.
2. Hoptics never changes another repeater. The Advisor suggests; owners apply.
3. Recommendation cards go only to owners who opted in.
4. Observer uploads only what any radio on the mesh can already hear.
5. No secrets in the repository. Keys, Wi-Fi, MQTT and Discord webhook live in encrypted storage.
6. A hop that can't be pinned to one repeater (2-byte prefix collision) is drawn dashed, never guessed.
7. No code or artwork copied from MeshOS, Cairn, WADAMESH or CoreScope (GPL v3). Ideas only.
8. **Alerts are never sent to the mesh automatically.** An alert pops up for 45 seconds with a
   ready-to-send message; only a person tapping *Send to #alerts* on an enrolled device sends it.
   One post per alert, never repeated, kept short to save airtime. What qualifies for `#alerts`
   is agreed with the SacMesh community first.
9. Dashboard data is never broadcast over LoRa. Displays get it over Wi-Fi from the plugin feed,
   so the mesh's airtime stays for people.

## How the pieces fit

```
 NWS / CAL FIRE / outages / SondeHub / space wx      your LAN: auto_rx Pi, WPSD hotspots, APRS
                     \                                   /
                      v                                 v
          +------------------------------------------------------+
          |  Hoptics openHop plugin (Pi repeater, Python)        |
          |  mesh stats, scoring, alert engine, signed feed,     |
          |  web dashboard                                       |
          +------------------------------------------------------+
                 | MQTT over TLS (LAN)         | HTTPS (LAN)
                 v                             v
     CrowPanel / Heltec / Cardputer      Cardputer Zero on HDMI
```

The plugin does all the outside fetching, so displays never call outside services themselves.
That keeps device code small, saves power, and shrinks what an attacker can reach.

Open question: check whether openHop's plugin interface can originate channel messages. If not,
*Send to #alerts* is sent by the display's own SX1262 (CrowPanel, Heltec) instead of the Pi.

## Alerts

| Source | Feed | Notes |
| --- | --- | --- |
| NWS | api.weather.gov alerts for the SMF counties | Clean, free, reliable |
| CAL FIRE | public incident list | Filter by distance from SMF |
| Power outages | SMUD and PG&E outage maps | No clean public feed; may break when their sites change |

The popup shows type, source, area and expiry, the exact message that would be sent, and
*Send*, *Next* and *Dismiss*. After 45 seconds it hides; the alert stays under the top-bar
alerts button until it expires.

## Security checklist (every device, every release)

- No default passwords: each device forces a unique PIN on first boot
- Secure Boot v2 + flash encryption + encrypted NVS on ESP32 boards; readback protection on nRF52
- Signed firmware updates only; no unauthenticated OTA
- No listening ports by default; devices only make outgoing MQTT connections over TLS with
  certificate checks and their own credentials; no plain-text fallback
- Feed messages are signed by the plugin; devices pin the plugin's public key at setup
- Web settings page off by default; LAN-only and password-protected when on
- JTAG and serial console locked in release builds
- Every over-the-air packet is treated as hostile; decoders are bounds-checked and fuzz-tested
- Rate limits on MQTT, Discord and mesh posts
- Cardputer Zero (Linux): SSH keys only, firewall on, dashboard runs as an unprivileged user,
  automatic security updates
- Dependencies pinned, with a license list

Warning: Secure Boot and flash encryption burn eFuses and cannot be undone. Test the release
flow on one spare board before doing it to the rest.

## Power

- The screen is the biggest drain: auto-dim, sleep when idle, wake on touch or a new alert
- Displays receive pushed updates; no polling loops on devices
- LVGL redraws only the parts that changed; lower frame rate on battery
- Two profiles: **mains** (full brightness, everything live) and **battery** (dimmed, slower updates,
  Wi-Fi power save)
- Wi-Fi and Bluetooth off when not needed; CPU frequency scaling on ESP32
