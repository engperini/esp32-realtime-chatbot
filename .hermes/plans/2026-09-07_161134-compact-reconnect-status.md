# Compact Centered Reconnection Status Plan

> **For Hermes:** Implement only after explicit user authorization.

**Goal:** Keep the offline display readable on the 115×72 round screen and call the unavailable service `Sem LLM`.

**Finding:** `status_label` currently has no explicit width or centered text alignment. It uses the 20px font, while `RECONECTANDO` is 12 characters and exceeds the approximately 86px-wide text overlay. Its initial `align_to(text_label, ...)` position is also not refreshed as content changes.

**Files:**
- Modify: `components/eye_animation/eye_animation.cpp`
- Modify: `main/main.cpp`

**Changes:**
1. In `main/main.cpp`, replace the unavailable API state with two short labels:
   - main text: `Sem LLM`
   - status: `TENTAR`
2. In `eye_animation.cpp`, make `status_label` width `100%` of the existing overlay, set `LV_TEXT_ALIGN_CENTER`, apply `LV_LABEL_LONG_CLIP`, and align it to the bottom center of the overlay rather than the dynamically changing main-text label.
3. Preserve the existing 20px PT-BR font and do not alter eye animation or portal behavior.
4. Build, run an ad-hoc temporary `hermes-verify-*.py`, flash `COM11`, and use the physical screen to confirm `Sem LLM` and `TENTAR` are centered and fully visible.

**Out of scope:** API migration and WebSocket/TLS correction.
