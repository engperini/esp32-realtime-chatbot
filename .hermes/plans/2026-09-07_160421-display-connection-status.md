# Honest Display Status Implementation Plan

> **For Hermes:** Implement only after explicit user authorization.

**Goal:** Make the round display distinguish boot, portal, Wi-Fi, WebSocket-connected, and WebSocket-error states using short text that fits the 115×72 display.

**Architecture:** Keep the existing `eye_set_text`/`eye_set_status` API. Change only status messages and transitions in `main/main.cpp`; do not touch the later OpenAI Realtime protocol migration.

**Tech Stack:** ESP-IDF 5.5, `esp_websocket_client`, LVGL eye-animation display.

---

### Task 1: Define concise display vocabulary

**Files:**
- Modify: `main/main.cpp`

Use maximum two short lines per state:

| State | Main text | Status line |
|---|---|---|
| boot | `Iniciando` | `CONECTANDO` |
| portal active | `Wi-Fi: esp32s3` | `IP 192.168.4.1` |
| WebSocket connected | `Conectado` | `API ONLINE` |
| WebSocket disconnected/error | `Sem API` | `RECONECTANDO` |

The portal password remains available in the portal documentation/UI flow, but is removed from the small round display to prevent wrapping/overflow.

### Task 2: Make WebSocket status truthful

**Files:**
- Modify: `main/main.cpp:609-842`

- On `WEBSOCKET_EVENT_CONNECTED`, show `Conectado` / `API ONLINE`.
- On `WEBSOCKET_EVENT_DISCONNECTED` and `WEBSOCKET_EVENT_ERROR`, show `Sem API` / `RECONECTANDO`.
- Do not display `READY` until the WebSocket is connected.

### Task 3: Shorten portal display content

**Files:**
- Modify: `main/main.cpp:844-864`

Replace the current multi-line `Portal`, SSID, password and long `Abra ...` message with the concise portal vocabulary above. Preserve portal behavior, WPA2 credentials, DNS and `192.168.4.1`.

### Task 4: Remove the misleading default label

**Files:**
- Modify: `components/eye_animation/eye_animation.cpp:367-371`

Change the initial hardcoded `READY` label to `INICIANDO`, so the display cannot briefly claim readiness before network/API initialization.

### Task 5: Verify and flash

1. Build with local ESP-IDF: `idf.py -C C:\c\Users\engpe\esp32-realtime-chatbot build`.
2. Run a temporary `hermes-verify-*.py` script from `C:\Users\engpe\AppData\Local\Temp` asserting the expected status transitions and short portal strings.
3. Flash `COM11` only with the existing user authorization.
4. Capture serial logs after reboot:
   - without an OpenAI WebSocket connection: display must show `Sem API` / `RECONECTANDO`;
   - when portal is active: log must show `SSID esp32s3`, DHCP `192.168.4.1`, and display text must remain short.

**Out of scope:** OpenAI Realtime protocol migration in README Priority 10. That is likely the reason for the TLS/WebSocket failure and will be planned separately after this display correction.
