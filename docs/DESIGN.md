# Bluffscope design notes

The full design spec lives in the shared doc:
https://claude.ai/artifact/1uUsMYUS5tBJD8BzwkpKUm

## Rules that never bend

1. SMF only: 909.875 MHz, BW 62.5 kHz, SF9, CR5. Not a setting.
2. Bluffscope never changes another repeater. The Advisor suggests; owners apply.
3. Recommendation cards go only to owners who opted in.
4. Observer uploads only what any radio on the mesh can already hear.
5. No secrets in the repository. Keys, Wi-Fi, MQTT and Discord webhook live in encrypted NVS.
6. A hop that can't be pinned to one repeater (2-byte prefix collision) is drawn dashed, never guessed.
7. No code or artwork copied from MeshOS, Cairn, WADAMESH or CoreScope (GPL v3). Ideas only.

## Security checklist (release)

- Secure boot v2 + flash encryption + NVS encryption
- MQTT over TLS with certificate checks, no plain-text fallback
- Packet decoder bounds-checked and fuzz-tested
- Signed firmware updates only
- Web settings page off by default; password-protected when on
- Rate limits on MQTT and Discord posts
- Dependencies pinned, with a license list
