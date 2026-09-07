# OpenAI Realtime GA Migration Plan

> **For Hermes:** Implement only after explicit user authorization.

**Goal:** Restore real ESP32↔OpenAI voice testing against the current official Realtime interface while preserving the direct WebSocket architecture, current PCM16 audio path, and existing barge-in code for a later focused fix.

**Evidence and constraint:** The physical device connects to Wi-Fi and receives `192.168.0.16`, but fails before any WebSocket handshake with `ESP_ERR_MBEDTLS_SSL_SETUP_FAILED` / `mbedtls_ssl_setup returned -0x7F00`. This happens below the Realtime protocol layer. Therefore the work begins with a TLS memory/configuration diagnosis; changing the model/event schema alone cannot prove a fix.

**Official documentation consulted:**
- https://developers.openai.com/api/docs/guides/realtime
- https://developers.openai.com/api/docs/guides/realtime-conversations
- https://developers.openai.com/api/reference/resources/realtime/client-events

The current official voice-agent guide lists `gpt-realtime-2.1` for low-latency speech-to-speech sessions and retains WebSocket as the suitable connection method for server/device-style clients.

---

### Task 1: Add non-secret TLS preflight diagnostics

**Files:**
- Modify: `main/main.cpp:1083-1146`

Before starting the WebSocket, log internal heap, largest free block, and free PSRAM. In the WebSocket error callback, log only ESP-TLS error codes and heap state—never headers or API keys.

**Acceptance:** determine whether `-0x7F00` is allocation pressure/configuration rather than an API authentication or event-schema rejection.

### Task 2: Make TLS configuration explicit and reduce transient pressure

**Files:**
- Modify: `main/main.cpp:1090-1107`
- Possibly modify: `sdkconfig.defaults` only if the local IDF configuration lacks a required TLS/CRT bundle option.

Keep `esp_crt_bundle_attach`, use the current direct `wss://api.openai.com/v1/realtime` connection, and place WebSocket buffers/tasks in appropriate memory only if diagnostics show allocation pressure. Do not weaken certificate validation or disable hostname verification.

**Acceptance:** obtain a completed TLS + HTTP WebSocket handshake, or produce a concrete TLS failure with memory evidence.

### Task 3: Update the Realtime model and GA session schema

**Files:**
- Modify: `main/main.cpp:77, 241-283`

After the handshake works, update the model target from the old pinned mini model to the current documented speech-to-speech model (`gpt-realtime-2.1`, subject to the account/model availability response). Update `session.update` to the current GA session shape, explicitly retaining:
- PCM16 input and output;
- `marin` voice only if accepted by the current model/session response;
- server VAD;
- Portuguese child-safe instructions;
- `create_response: false`, pending the explicit existing `response.create` flow.

**Acceptance:** server accepts `session.update` without an `error` event.

### Task 4: Align response and audio event handling with GA events

**Files:**
- Modify: `main/main.cpp:268-367, 660-831`

Compare every emitted and consumed event to the official client/server reference. Retain `input_audio_buffer.append`, server VAD committed flow, `response.create`, PCM16 audio decoding, and `response.done` lifecycle only where still supported. Add targeted logging of server `error` code/message/type without serializing sensitive request headers.

**Acceptance:** a short spoken turn produces: VAD start → VAD commit → `response.create` → audio delta → playback → `response.done`.

### Task 5: Physical acceptance test, excluding barge-in changes

**Files:** no functional changes unless a protocol mismatch is found.

1. Build with local ESP-IDF 5.5.
2. Run focused temporary `hermes-verify-*.py` source checks under `C:\Users\engpe\AppData\Local\Temp` and remove it afterwards.
3. Flash COM11.
4. Verify a short Portuguese request produces output audio.
5. Capture a redacted serial log with connection/session/event lifecycle.

**Explicitly deferred:** interruption while the LLM is speaking. The current code uses `BARGE_IN_MANUAL`, has `response.cancel`, and deliberately has `conversation.item.truncate` disabled. That behavior will be isolated and repaired only after ordinary end-to-end conversation works on the new Realtime protocol.
