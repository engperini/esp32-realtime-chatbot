from pathlib import Path

PROJECT = Path(r"C:\c\Users\engpe\esp32-realtime-chatbot")
DEFAULTS = (PROJECT / "sdkconfig.defaults").read_text(encoding="utf-8")
MAIN = (PROJECT / "main/main.cpp").read_text(encoding="utf-8")

assert "CONFIG_MBEDTLS_DYNAMIC_BUFFER=y" in DEFAULTS
assert "CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN=4096" in DEFAULTS
assert "ws_cfg.buffer_size = 4096;" in MAIN
assert "log_tls_memory(" in MAIN
assert "gpt-realtime-2.1" in MAIN
assert "OpenAI-Beta" not in MAIN
assert "data == nullptr || data->data_ptr == nullptr || data->data_len <= 0" in MAIN
assert "MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT" in MAIN
assert "Falha ao alocar mensagem de áudio" in MAIN
assert "ws_audio_frames_sent" in MAIN
assert "ws_audio_frames_backpressure" in MAIN
assert "ws_audio_frames_dropped" in MAIN
assert "tentativa <= 2" in MAIN

print("PASS: TLS allocation budget and GA WebSocket safeguards are explicit")
