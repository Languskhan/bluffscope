# Hoptics data feed (draft)

The openHop plugin publishes everything displays need over MQTT on your LAN. Displays only
subscribe; the one thing they can send is a *Send this alert* request.

## Transport

- A broker (Mosquitto) runs next to the plugin on the Pi, LAN only, TLS on.
- Each display has its own username and password, set during first-boot setup.
- Broker rules: displays may **read** `hoptics/v1/#` and **write** only to their own
  `hoptics/v1/smf/cmd/<device-id>` topic.
- Snapshot topics are retained, so a display that wakes up gets current data at once.

## Topics

| Topic | Contents | Rate |
| --- | --- | --- |
| `hoptics/v1/smf/health` | score, repeaters online/total, packets/hr, SNR, airtime, dupes | 1 min |
| `hoptics/v1/smf/repeaters` | ranked list: id, name, position, score, SNR, last heard | 5 min |
| `hoptics/v1/smf/traffic` | one message per packet heard: type, channel, path, SNR | live |
| `hoptics/v1/smf/channel` | 24 h packets/hr and noise floor series | 10 min |
| `hoptics/v1/smf/alerts` | active alerts: id, source, type, area, expiry, ready-to-send text | on change |
| `hoptics/v1/smf/balloon` | sonde id, position, altitude, rate, burst and landing prediction | live while tracking |
| `hoptics/v1/smf/aprs` | your beacons and messages | live |
| `hoptics/v1/smf/hotspots` | per hotspot: online, last-heard mode, talkgroup/room, duration | live |
| `hoptics/v1/smf/wx` | local weather | 10 min |
| `hoptics/v1/smf/spacewx` | SFI, SSN, Kp, X-ray, solar wind, Bz, band conditions | 15 min |
| `hoptics/v1/smf/iss` | next pass times, max elevation, directions | 1 h |
| `hoptics/v1/smf/cmd/<device-id>` | display to plugin: `{"send_alert": "<id>"}` | on tap |

## Message shape

Compact JSON. Every message carries:

```json
{ "v": 1, "ts": 1795420267, "src": "hoptics-plugin", "sig": "<ed25519 signature>", "d": { } }
```

- `sig` signs `v`, `ts`, `src` and `d`. Displays pin the plugin's public key at setup and drop
  anything unsigned, badly signed or older than a few minutes.
- Lite devices can ask for a trimmed version (`hoptics/v1/smf/lite/...`) with short field names.

## Sending an alert

1. The plugin publishes an alert with its ready-to-send text.
2. A person taps *Send to #alerts* on an enrolled display within its 45-second popup (or later
   from the alerts list).
3. The display publishes `{"send_alert": "<id>"}` to its own cmd topic.
4. The plugin checks: known alert, not already sent, rate limit OK, then sends it once
   through its openHop Companion identity on `#alerts`.
5. Everyone's popup marks it *Sent*.
