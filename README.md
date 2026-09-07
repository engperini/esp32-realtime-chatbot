# ESP32 Realtime Chatbot

Standalone voice assistant prototype built around the **Seeed Studio XIAO ESP32-S3 Sense**, **Seeed Studio Round Display for XIAO**, a **MAX98357A I2S amplifier**, microphone, speaker and animated LVGL interface.

The main design goal is to run the conversational audio path **directly on the ESP32-S3**, without maintaining a separate application server for streaming audio. The ESP32 captures microphone audio, connects to the OpenAI Realtime API over TLS/WebSocket, sends PCM16 audio, receives generated audio and plays it through I2S.

> **Project status:** working prototype / active development. The current firmware is a backup of the working hardware implementation. The roadmap below documents known improvements before changing the working architecture.

---

## 1. Project goals

- Direct ESP32-S3 -> OpenAI Realtime connection.
- No dedicated audio relay/server required for the personal prototype.
- Full-duplex conversational audio.
- Server-side VAD plus local speech detection for barge-in.
- PDM microphone input from the XIAO ESP32-S3 Sense.
- I2S audio output through MAX98357A and speaker.
- Round LCD with LVGL animated eyes and conversation status.
- FreeRTOS tasks separating capture, network encoding, playback and UI.
- Future Wi-Fi/API-key provisioning without recompiling firmware.

> **Security note:** direct use of a permanent API key inside firmware is acceptable only for a controlled personal prototype. A distributable product should not expose a long-lived API key in firmware; see the roadmap.

---

## 2. Current architecture

```text
                 +----------------------------+
                 | XIAO ESP32-S3 Sense        |
                 |                            |
PDM microphone ->| I2S/PDM RX                 |
                 |       |                    |
                 |       v                    |
                 | PCM16 -> Base64            |
                 |       |                    |
                 |       v                    |
                 | TLS WebSocket              |
                 +-------|--------------------+
                         |
                         v
                 OpenAI Realtime API
                         |
                         | Base64 / PCM16 audio
                         v
                 +----------------------------+
                 | ESP32-S3                   |
                 | audio queue                |
                 | mono -> stereo             |
                 | I2S TX                     |
                 +-------|--------------------+
                         |
                         v
                    MAX98357A
                         |
                         v
                       Speaker

                 ESP32-S3 -> SPI -> Round LCD
                         -> LVGL
                         -> animated eyes/status
```

Application states currently include **IDLE**, **LISTENING**, **THINKING** and **SPEAKING**.

---

## 3. Hardware

### Main devices

| Device | Purpose |
|---|---|
| Seeed Studio XIAO ESP32-S3 Sense | Main MCU + PDM microphone |
| Seeed Studio Round Display for XIAO | 240x240 LCD / LVGL UI |
| MAX98357A | I2S digital audio amplifier |
| Speaker | Voice output |
| Optional servo / actuator | Reserved future feature |

---

## 4. Complete pin map

The table below documents the **actual project allocation**, including deliberate hardware modifications.

| XIAO pin | ESP32-S3 GPIO | Current use | Device / signal | Notes |
|---|---:|---|---|---|
| D0 | GPIO1 | Available / future | - | Currently not part of the main audio/display path |
| D1 | GPIO2 | LCD CS in firmware definition | Round Display LCD_CS | **Current hardware does not route this signal to the ESP32. LCD CS is tied directly to GND, keeping the LCD permanently selected.** Firmware should eventually be made consistent with the physical implementation. |
| D2 | GPIO3 | I2S WS/LRCLK | MAX98357A | Round Display normally assigns D2 to microSD CS. **microSD is intentionally not used**, allowing GPIO3 to be reused for audio. Future cleanup: configure `usd_cs` as NC so software matches the hardware intent. |
| D3 | GPIO4 | LCD D/C | Round Display LCD_DC | SPI display data/command selection |
| D4 | GPIO5 | I2C SDA | Round Display | Used by Round Display board functions; keep reserved for display/I2C |
| D5 | GPIO6 | I2C SCL | Round Display | Used by Round Display board functions; keep reserved for display/I2C |
| D6 | GPIO43 | I2S audio data out | MAX98357A DIN | Round Display normally uses D6 for LCD backlight. **Backlight has been physically isolated/sectioned and powered directly**, deliberately freeing GPIO43 for I2S audio. Firmware sets LCD backlight to NC. |
| D7 | GPIO44 | Reserved future servo/motor | Servo PWM / future actuator | Round Display normally exposes touch interrupt here. **Touch is intentionally not used.** Keep documented before enabling servo code. |
| D8 | GPIO7 | SPI SCK | Round Display LCD | LCD clock |
| D9 | GPIO8 | I2S BCLK | MAX98357A | Display MISO is not used by the current LCD configuration, freeing GPIO8 for I2S BCLK. Verify before adding any SPI peripheral requiring MISO. |
| D10 | GPIO9 | SPI MOSI | Round Display LCD | LCD pixel/command data |

### MAX98357A connection

| MAX98357A | ESP32-S3 | Function |
|---|---:|---|
| BCLK | GPIO8 / D9 | I2S bit clock |
| LRC / WS | GPIO3 / D2 | I2S word select |
| DIN | GPIO43 / D6 | I2S audio data |
| GND | GND | Common ground |
| VIN | Power rail | Amplifier supply according to the hardware build |

### Round Display connection used by the firmware

| LCD signal | ESP32-S3 | Notes |
|---|---:|---|
| LCD_CS | Permanently GND in current hardware | LCD always selected; GPIO2/D1 is not physically connected to LCD CS in this custom build |
| LCD_DC | GPIO4 / D3 | Data/command |
| SCK | GPIO7 / D8 | SPI clock |
| MOSI | GPIO9 / D10 | SPI data to LCD |
| MISO | NC | Not required for current LCD use |
| Backlight | Direct hardware connection | D6/GPIO43 physically freed for MAX98357A |
| microSD CS | Not used | D2/GPIO3 intentionally reused for MAX98357A WS |
| Touch INT | Not used | D7/GPIO44 reserved for future actuator |

### Important hardware modifications

This project is **not a completely stock plug-and-play Round Display wiring arrangement**.

1. **LCD CS:** the display CS connection is not routed to ESP32 GPIO2 in the current custom board. LCD CS is tied directly to **GND**, keeping the display selected.
2. **Backlight / D6:** the Round Display backlight connection has been physically sectioned/isolated and the backlight is powered directly. This releases **GPIO43 / D6** for MAX98357A I2S data.
3. **microSD:** intentionally unused. Its normal CS pin, **GPIO3 / D2**, is reused as MAX98357A I2S WS/LRCLK.
4. **Touch:** intentionally unused. **GPIO44 / D7** remains available for a future servo/motor output.

These modifications must be understood before reproducing the hardware.

---

## 5. Firmware structure

```text
esp32-realtime-chatbot/
├── main/
│   ├── main.cpp
│   ├── CMakeLists.txt
│   └── idf_component.yml
├── components/
│   ├── eye_animation/
│   │   ├── eye_animation.cpp
│   │   └── eye_animation_engine.cpp
│   ├── ui_display/
│   │   └── display.cpp
│   ├── servo_control/
│   │   └── servo_control.cpp
│   └── ...
├── sdkconfig.defaults
├── partitions.csv
└── README.md
```

### `main/main.cpp`

Core application orchestration:

- Wi-Fi station initialization.
- TLS WebSocket client.
- OpenAI Realtime session/events.
- PDM microphone capture.
- Base64 encoding and network transmission.
- Realtime response/event processing.
- Audio decoding/queueing/playback.
- MAX98357A I2S TX.
- server/local VAD handling.
- barge-in logic.
- FreeRTOS task creation.

### `components/eye_animation/`

Owns the display/LVGL lifecycle and animated eye engine.

### `components/ui_display/`

Compatibility/public display API. The actual display lifecycle is owned by `eye_animation`; text/status calls are redirected to it.

### `components/servo_control/`

Experimental/future servo control. Servo task is currently disabled. GPIO44/D7 is reserved for this possibility because touch input is not used.

### `sdkconfig.defaults`

Important configuration includes ESP32-S3 at 240 MHz, 8 MB flash, Octal PSRAM at 80 MHz, TLS certificate bundle, WebSocket client, LVGL and custom partitions.

### `partitions.csv`

Defines NVS, PHY and factory application partitions. NVS is planned for persistent Wi-Fi and API configuration.

---

## 6. Current FreeRTOS/data flow

1. **Microphone capture task** — reads PCM16 from the PDM microphone and queues chunks.
2. **Microphone encode/send task** — Base64 encodes microphone chunks and sends them over the Realtime WebSocket.
3. **Audio playback task** — receives decoded model audio, converts mono to stereo and writes to MAX98357A through I2S.
4. **Eye/display task** — owns LVGL and processes UI commands through its own queue.
5. **Boot beep task** — provides local startup audio feedback.

This architecture should be preserved unless profiling demonstrates a reason to change it.

---

# 7. Development roadmap / known corrections

The items below are deliberately documented **before changing the current working firmware**.

## Priority 1 — Make firmware pin configuration match the real hardware

### 1.1 LCD CS permanently active

**Current hardware:** LCD CS is tied to GND and is not physically connected to GPIO2/D1.

**Current firmware:** still declares `lcd_cs = GPIO_NUM_2`.

**Plan:** configure the display driver so the software explicitly represents a permanently selected LCD / no controllable CS, if supported by the display component. Do not blindly change this until the library behavior is verified.

### 1.2 microSD CS / GPIO3

**Current design is intentional:** microSD is not used and GPIO3/D2 is used as MAX98357A WS/LRCLK.

**Plan:** change the Round Display configuration from `usd_cs = GPIO_NUM_3` to NC/disabled if supported.

**Reason:** prevent the display library from ever reconfiguring GPIO3 and interfering with I2S.

### 1.3 Backlight / GPIO43

**Current design is intentional:** the Round Display backlight connection is physically isolated and powered directly, while GPIO43/D6 carries MAX98357A I2S data.

**Plan:** keep `lcd_backlight` disabled/NC in firmware and document the physical modification permanently.

### 1.4 GPIO44 / touch / future servo

**Current design:** touch is not used. Servo task is disabled.

**Plan:** reserve GPIO44/D7 for a future actuator and leave an explicit warning in source code before enabling servo support.

---

## Priority 2 — Fix audio queue memory leak during barge-in

Audio queue items contain dynamically allocated buffers. The current interruption path can call:

```cpp
xQueueReset(audio_queue);
```

FreeRTOS discards queue entries when a queue is reset, but it does not free memory referenced by pointers stored inside those entries. Therefore queued audio buffers can become unreachable after repeated barge-ins.

**Planned correction:** drain the queue and explicitly free every queued buffer before resetting/reusing it.

```cpp
audio_chunk_t chunk;
while (xQueueReceive(audio_queue, &chunk, 0) == pdTRUE) {
    heap_caps_free(chunk.data);
}
```

Then reset playback state safely.

**Why it matters:** repeated conversational interruptions could otherwise gradually consume PSRAM and eventually cause allocation failures or unstable audio.

---

## Priority 3 — Correct playback time used by `conversation.item.truncate`

Playback bytes are counted after mono audio has been expanded to stereo.

For 24 kHz, 16-bit, stereo PCM:

```text
24000 samples/s x 2 bytes/sample x 2 channels = 96000 bytes/s
```

The current calculation effectively assumes 48,000 bytes/s, which can report approximately twice the actual playback duration.

**Plan:** calculate elapsed playback time using the actual output format:

```cpp
played_ms = (played_bytes * 1000ULL) /
            (AUDIO_SAMPLE_RATE * sizeof(int16_t) * 2);
```

This becomes particularly important when `conversation.item.truncate` is enabled so the server conversation state matches what the user actually heard before an interruption.

---

## Priority 4 — Improve local VAD hysteresis and hold time

The firmware already defines ON/OFF thresholds and a hold interval, but the current local `user_speaking` decision primarily uses the ON threshold and the hold behavior is disabled/commented.

**Plan:** implement a real stateful detector:

```text
NOT SPEAKING:
    RMS > threshold_on -> SPEAKING

SPEAKING:
    keep speaking while signal remains active
    RMS below threshold_off for ~300 ms -> NOT SPEAKING
```

Use separate ON/OFF thresholds to create hysteresis.

**Expected benefit:** fewer false barge-ins and less frame-to-frame oscillation near the RMS threshold.

Server VAD should remain available; local VAD is primarily useful for immediate interruption behavior.

---

## Priority 5 — Make barge-in flush local playback safely and immediately

Stopping server generation is only part of an interruption. Audio already buffered on the ESP32 may continue playing after the user starts talking.

**Plan:** when a valid interruption is detected:

1. Stop accepting/playing the current response.
2. Cancel the current response when appropriate.
3. Drain and free queued audio buffers.
4. Stop/flush local playback as quickly as the I2S API safely allows.
5. Calculate the actual amount heard.
6. Use `conversation.item.truncate` when protocol support is enabled.
7. Reset state for the next response.

Goal: make interruption feel immediate without introducing memory leaks or corrupting conversation state.

---

## Priority 6 — Fix silence-frame constant/comment mismatch

The source describes a 10 ms silence block while using 48 frames at 24 kHz:

```text
48 / 24000 = 2 ms
```

A true 10 ms block at 24 kHz is:

```text
240 frames
```

**Plan:** decide whether 2 ms or 10 ms is desired, then make the constant and documentation agree.

---

## Priority 7 — Reduce repeated dynamic allocation in the audio hot path

Current capture/playback paths repeatedly allocate and free PCM, Base64 and stereo buffers.

**Plan:** investigate fixed-size buffer pools/ring buffers for:

- microphone PCM chunks;
- Base64 encode workspace;
- decoded response audio;
- stereo playback workspace.

**Goal:** reduce heap fragmentation, allocation latency and long-running instability while preserving the current FreeRTOS architecture.

---

## Priority 8 — Make WebSocket message assembly robust

The current JSON completeness logic counts `{` and `}` characters. Braces appearing inside a JSON string can theoretically confuse that method.

**Plan:** use WebSocket message/frame metadata from `esp_websocket_client` to determine when the complete message has arrived, then parse JSON only after the full payload is assembled.

---

## Priority 9 — Wi-Fi + API configuration portal

### Problem

Wi-Fi SSID/password and OpenAI API configuration should not require editing source code and reflashing firmware whenever the device changes network or credentials.

### Planned boot behavior

```text
Read configuration from NVS
        |
        +-- valid Wi-Fi config -> attempt STA connection
        |                         |
        |                         +-- success -> start Realtime client
        |                         |
        |                         +-- repeated failure -> setup mode
        |
        +-- no configuration -> setup mode
```

### Setup mode

ESP32 creates a temporary access point, for example:

```text
SSID: ESP32-Realtime-Setup
```

The user connects with a phone/computer and opens a local configuration page/captive portal.

Planned fields:

- Wi-Fi network / SSID.
- Wi-Fi password.
- OpenAI API key.
- Optional device name.

Planned actions:

- Scan nearby Wi-Fi networks.
- Save settings to NVS.
- Validate required fields.
- Restart/connect using the new configuration.
- Reopen setup mode later without reflashing.
- Factory/configuration reset mechanism to erase saved Wi-Fi/API credentials.

### Storage

Use NVS namespaces instead of compile-time `#define` credentials.

Credentials must never be printed in normal logs.

### API-key security

For the current personal prototype, storing the API key locally in NVS is the simplest serverless approach.

For a future product distributed to other users, do **not** ship a permanent OpenAI API key in firmware. Evaluate an ephemeral-token/bootstrap architecture where a minimal trusted service issues temporary credentials while the actual Realtime audio connection remains direct from device to OpenAI.

---

## Priority 10 — Migrate the Realtime protocol/API version

The project was built against an earlier Realtime WebSocket interface/model version.

**Plan:**

1. Compare the current OpenAI Realtime GA protocol with the events used in `main.cpp`.
2. Update the Realtime model identifier.
3. Update `session.update` payload structure.
4. Update renamed audio/audio-transcript events as required.
5. Verify VAD behavior.
6. Verify response creation/cancellation.
7. Verify barge-in and truncation against the current API.
8. Test long conversations on the physical ESP32-S3.

**Important:** preserve the direct ESP32 -> Realtime architecture unless the current API technically requires otherwise.

---

## Priority 11 — OTA and maintainability

After provisioning and Realtime migration are stable, investigate:

- OTA firmware updates.
- Recovery/rollback strategy.
- Version information on the configuration page.
- Device diagnostics without exposing credentials.
- Wi-Fi signal/audio buffer/heap diagnostics.
- Watchdog/reconnect soak testing.

The current partition layout will need review before implementing a production-quality OTA scheme.

---

## 8. Recommended implementation order

```text
[Current working hardware baseline]
        |
        v
1. Document + reconcile GPIO configuration
        |
2. Fix audio queue memory ownership
        |
3. Fix playback/truncate timing
        |
4. Improve VAD + barge-in
        |
5. Improve buffer management
        |
6. Robust WebSocket assembly
        |
7. Wi-Fi/API captive configuration portal + NVS
        |
8. Realtime API migration
        |
9. OTA / recovery / long-duration testing
```

Each stage should be committed separately so the known-working version remains easy to recover.

---

## 9. Things intentionally NOT used in the current build

- Round Display microSD interface.
- Round Display touch interrupt/input.
- Round Display GPIO-controlled backlight.
- Servo task (currently future/experimental).
- Dedicated external application server for the Realtime audio stream.

---

## 10. Development principles

1. **Do not break the known-working audio path while cleaning up unrelated code.**
2. Hardware modifications are part of the design and must remain documented.
3. Prefer small, independently testable commits.
4. Test changes on the physical XIAO ESP32-S3 Sense + Round Display hardware.
5. Preserve direct Realtime communication as a core project goal.
6. Treat buffer ownership explicitly when FreeRTOS queues contain pointers.
7. Never commit real Wi-Fi credentials or API keys.
8. Keep experimental servo/touch functionality disabled until GPIO ownership is explicitly changed.

---

## 11. Current status summary

### Working / implemented

- ESP32-S3 Wi-Fi client.
- TLS WebSocket Realtime connection.
- PDM microphone capture.
- Continuous microphone streaming.
- Server VAD handling.
- Local RMS speech detection.
- Realtime response audio playback.
- MAX98357A I2S output.
- Animated Round Display interface.
- LVGL status/text integration.
- Initial barge-in/cancel architecture.
- PSRAM-enabled ESP-IDF configuration.

### Planned

- GPIO configuration cleanup matching custom hardware.
- Queue memory-leak correction.
- Accurate truncate/playback timing.
- Improved VAD hysteresis.
- Deterministic buffer management.
- Robust WebSocket message assembly.
- Wi-Fi captive portal/provisioning.
- API-key configuration through the same portal.
- NVS credential storage.
- Realtime API/model migration.
- OTA/recovery support.
- Long-duration stability testing.

---

## License / usage

No license has been explicitly defined yet. Add an appropriate license before treating the repository as a reusable public project.
