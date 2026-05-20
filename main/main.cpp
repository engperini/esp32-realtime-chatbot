// =========================
// Realtime Voice (ESP32-S3) — MINIMAL, SERVER VAD ONLY
// - Mic always streaming (append)
// - Server VAD decides turns
// - On committed -> response.create
// - Barge-in: when server sends speech_started while model speaking -> stop local playback + response.cancel (+ optional truncate)
// =========================

//PIN USED ESP32S3 + ROUND DISPLAY
//DISPLAY
// cfg.sda = GPIO_NUM_5;
// cfg.scl = GPIO_NUM_6;
// cfg.usd_cs = GPIO_NUM_3; (VERIFICAR NECESSIDADE POIS NAO ESTOU USANDO CARTÃO SD)
// cfg.lcd_cs = GPIO_NUM_2;
// cfg.lcd_dc = GPIO_NUM_4;
// cfg.mosi = GPIO_NUM_9;
// cfg.sck  = GPIO_NUM_7;
// cfg.touch_interrupt = GPIO_NUM_44;


// Mic PDM RX (XIAO ESP32-S3 SENSE)
//I2S_PDM_CLK GPIO_NUM_42
//I2S_PDM_DIN GPIO_NUM_41

//NEW I2S STD AUDIO TX
//I2S_LRC   GPIO_NUM_3  
//I2S_BCLK  GPIO_NUM_8
//I2S_DIN   GPIO_NUM_43

//SERVO
//PWM_OUT GPIO_NUM_44

//AALOG POTENTIOMETER
//ADC1_CHANNEL_0 // (GPIO1)



#include "servo_control.h"
//#include "display_status.h"
#include "display.h"
#include "eye_animation.h"


//#include "seeed-studio-round-display.hpp"

#include "lvgl.h"

#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "esp_wifi.h"
#include "esp_netif.h"

#include "esp_websocket_client.h"
#include "esp_crt_bundle.h"

#include "driver/gpio.h"
#include "esp_timer.h"

#include "cJSON.h"
#include "mbedtls/base64.h"

#include "driver/i2s_pdm.h"
#include "driver/i2s_std.h" //new was i2s

// =================== CONFIG ===================
#define WIFI_SSID "...."
#define WIFI_PASS "..."

// 
#define OPENAI_KEY "...."

// Model Realtime
#define REALTIME_URI "wss://api.openai.com/v1/realtime?model=gpt-realtime-mini-2025-12-15"

// Mic PDM RX (XIAO ESP32-S3 SENSE)
#define I2S_PDM_CLK GPIO_NUM_42
#define I2S_PDM_DIN GPIO_NUM_41

// Audio out PDM TX
// #define I2S_   I2S_NUM_0
// #define I2S_PDM_TX_CLK    GPIO_NUM_8
// #define I2S_PDM_TX_DOUT   GPIO_NUM_43

#define AUDIO_SAMPLE_RATE 24000

// Mic frames
#define MIC_FRAME_BYTES   2048
#define MIC_QUEUE_LEN     8

// Playback queue
#define AUDIO_QUEUE_LEN   50

// ===== AUDIO SILENCE (STEREO) =====
// 10 ms de silêncio em 24 kHz → 240 frames → 240 * 2 canais
#define SILENCE_FRAMES 48

static int16_t silence_stereo[SILENCE_FRAMES * 2] = {0};

#define MIN_VALID_AUDIO_BYTES 8000   // ~250 ms @16kHz mono 16-bit

static int32_t rms = 0;

// WS accumulator (json fragmentado)
#define WS_ACCUM_MAX 65536

static const char *TAG = "RT_MIN";

// =================== APP STATE ===================
typedef enum {
    APP_IDLE = 0,       // conectado, silêncio
    APP_LISTENING,      // server VAD detectou fala
    APP_THINKING,       // audio committed, aguardando resposta
    APP_SPEAKING        // modelo enviando áudio
} app_state_t;

static volatile app_state_t app_state = APP_IDLE;

static void app_set_state(app_state_t s) {
    if (app_state == s) return;
    app_state = s;
    const char *names[] = {"IDLE", "LISTENING", "THINKING", "SPEAKING"};
    ESP_LOGI("APP_STATE", "→ %s", names[s]);
}

// =================== GLOBALS ===================
static volatile bool ws_connected = false;
static bool ws_started = false;
static esp_websocket_client_handle_t client = NULL;

// I2S handles
static i2s_chan_handle_t rx_handle = NULL;
static i2s_chan_handle_t tx_handle = NULL;

// WS accum
static char   ws_accum[WS_ACCUM_MAX];
static size_t ws_accum_len = 0;

// Mic queue
typedef struct {
    uint8_t *pcm;
    size_t   len;
} mic_chunk_t;

static QueueHandle_t mic_queue = NULL;

// Audio queue
typedef struct {
    uint8_t *data;
    size_t   len;
} audio_chunk_t;

static QueueHandle_t audio_queue = NULL;

// Playback tracking for optional truncate
static volatile bool playback_abort = false;
static volatile uint64_t played_bytes_total = 0;
static volatile uint64_t played_bytes_item_start = 0;

static char current_response_id[64] = {0};
static char last_model_item_id[64] = {0};
static int  last_model_content_index = 0;
static bool audio_item_started = false;

static volatile bool model_speaking = false;

static volatile bool user_speaking = false;
static volatile int64_t last_voice_ts = 0;
static bool cancel_sent_for_response = false;

//local vad
#define VAD_THRESHOLD_ON   1400 // liga
#define VAD_THRESHOLD_OFF  1300 // desliga
#define VAD_BOOST         1000  // boost quando o modelo está falando


typedef enum {
    BARGE_IN_MANUAL = 0,
    BARGE_IN_SERVER
} barge_in_mode_t;

// 🔁 MUDE AQUI para testar
static barge_in_mode_t barge_in_mode = BARGE_IN_MANUAL;
//static barge_in_mode_t barge_in_mode = BARGE_IN_SERVER;

//servo with potentiometer
// static ServoControl steering;

// extern "C" void servo_task(void *param)
// {
//     steering.begin(GPIO_NUM_44, ADC1_CHANNEL_0);

//     while (true) {
//         steering.update();
//         vTaskDelay(pdMS_TO_TICKS(20)); // 50 Hz
//     }
// }



// =================== HELPERS ===================
static bool json_is_complete(const char *buf, size_t len)
{
    int braces = 0;
    for (size_t i = 0; i < len; i++) {
        if (buf[i] == '{') braces++;
        else if (buf[i] == '}') braces--;
    }
    return (braces == 0 && len > 0);
}

static void send_ws_text(const char *json)
{
    if (!client || !ws_connected || !json) return;
    esp_websocket_client_send_text(client, json, strlen(json), portMAX_DELAY);
}

static void send_ws_audio_append(const char *b64)
{
    if (!client || !ws_connected || !b64) return;

    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "type", "input_audio_buffer.append");
    cJSON_AddStringToObject(msg, "audio", b64);

    char *json = cJSON_PrintUnformatted(msg);
    esp_websocket_client_send_text(client, json, strlen(json), portMAX_DELAY);

    cJSON_Delete(msg);
    free(json);
}

// ===== session.update (server vad) =====
// Importante: input_audio_format/output_audio_format pcm16
static void ws_send_session_update(void)
{
    const char *msg =
        "{"
          "\"type\":\"session.update\","
          "\"session\":{"
            "\"instructions\":\"O seu nome é EVA.  Sempre responda em português usando linguagem simples, frases curtas e no máximo três frases por resposta. Fale de forma alegre, gentil e protetora, como uma robô amiga. Só responda se o áudio estiver claro; se não estiver, diga apenas que não entendeu e peça para falar de novo. Quando a criança pedir uma história, conte histórias infantis felizes e imaginativas, em partes curtas, uma parte por vez, perguntando ao final se quer ouvir a próxima parte. Use frases fixas como Quer ouvir uma história? Se o user pedir curiosidades, responda com curiosidades simples e divertidas sobre animais, espaço, robôs, cores, natureza ou amizade, sem explicar como você pesquisou.\","
            "\"voice\":\"marin\","
            "\"input_audio_format\":\"pcm16\","
            "\"output_audio_format\":\"pcm16\","
            "\"turn_detection\":{"
              "\"type\":\"server_vad\","
              "\"threshold\": 0.8,"
              "\"prefix_padding_ms\": 300,"
              "\"silence_duration_ms\": 1000,"
              "\"create_response\": false,"
              "\"interrupt_response\": true"
            "}"
          "}"
        "}";

    send_ws_text(msg);
    ESP_LOGI(TAG, "session.update enviado (server_vad)");
}

// ===== response.create =====
// Com server_vad: dispare isso quando receber input_audio_buffer.committed
static void ws_send_response_create(void)
{
    const char *msg =
        "{"
          "\"type\":\"response.create\","
          "\"response\":{"
            "\"instructions\":\"\","
            "\"modalities\":[\"audio\",\"text\"]"
          "}"
        "}";

    send_ws_text(msg);
    ESP_LOGI(TAG, "response.create enviado");
}

// ===== response.cancel =====
// cancela a resposta corrente
static void ws_response_cancel(void)
{
    // Alguns exemplos aceitam cancel sem response_id; você já usa com id.
    // Aqui, se tiver id, manda com id. Se não tiver, manda simples.
    if (current_response_id[0]) {
        char msg[256];
        snprintf(msg, sizeof(msg),
            "{"
              "\"type\":\"response.cancel\","
              "\"response_id\":\"%s\""
            "}",
            current_response_id
        );
        send_ws_text(msg);
        ESP_LOGI(TAG, "response.cancel enviado id=%s", current_response_id);
    } else {
        send_ws_text("{\"type\":\"response.cancel\"}");
        ESP_LOGI(TAG, "response.cancel enviado (sem id)");
    }
}

static void ws_send_input_audio_buffer_clear(void)
{
    if (!ws_connected) return;

    const char *msg =
        "{"
        "\"type\":\"input_audio_buffer.clear\""
        "}";

    esp_websocket_client_send_text(client, msg, strlen(msg), portMAX_DELAY);
    ESP_LOGI(TAG, "input_audio_buffer.clear enviado");
}


// ===== optional truncate =====
// se você quer “cortar” o item do modelo no ponto tocado (melhor UX)
static void send_conversation_item_truncate(const char *item_id, int content_index, uint32_t audio_end_ms)
{
    if (!ws_connected || !client) return;
    if (!item_id || !item_id[0]) return;

    char msg[256];
    snprintf(msg, sizeof(msg),
        "{"
          "\"type\":\"conversation.item.truncate\","
          "\"item_id\":\"%s\","
          "\"content_index\":%d,"
          "\"audio_end_ms\":%u"
        "}",
        item_id, content_index, (unsigned)audio_end_ms
    );

    send_ws_text(msg);
    ESP_LOGI(TAG, "truncate enviado item=%s end_ms=%u", item_id, (unsigned)audio_end_ms);
}

// ===== BARGE-IN (server VAD speech_started) =====
static void on_server_speech_started_interrupt(void)
{
    // 1) Para playback local imediatamente
    playback_abort = true;
    if (audio_queue) xQueueReset(audio_queue);

     cancel_sent_for_response = true; 

    // 2) Cancela resposta atual
    ws_response_cancel();
    //ws_send_input_audio_buffer_clear();

    // 3) Trunca item do modelo (opcional, melhora consistência)
    if (last_model_item_id[0]) {
        uint64_t played_bytes_in_item = played_bytes_total - played_bytes_item_start;
        uint32_t played_ms = (played_bytes_in_item * 1000) / (AUDIO_SAMPLE_RATE * 2);

        int32_t safe_ms = (int32_t)played_ms - 80;
        if (safe_ms < 0) safe_ms = 0;

        //send_conversation_item_truncate(last_model_item_id, last_model_content_index, (uint32_t)safe_ms);
    }
}

// =================== AUDIO PLAYBACK ===================
static void play_audio_base64_pcm16(const char *b64)
{
    if (!b64 || !audio_queue) return;

    size_t b64_len = strlen(b64);
    if (b64_len < 4) return;

    size_t out_len_max = (b64_len * 3) / 4 + 4;

    uint8_t *pcm = (uint8_t *)heap_caps_malloc(out_len_max, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pcm) return;

    size_t pcm_len = 0;
    int ret = mbedtls_base64_decode(
        pcm, out_len_max, &pcm_len,
        (const unsigned char *)b64, b64_len
    );

    if (ret != 0 || pcm_len < 2) {
        heap_caps_free(pcm);
        return;
    }

    pcm_len &= ~1; // alinhamento 16-bit

    audio_chunk_t chunk = {.data = pcm, .len = pcm_len};
    if (xQueueSend(audio_queue, &chunk, pdMS_TO_TICKS(50)) != pdTRUE) {
        heap_caps_free(pcm);
    }
}

static void audio_play_task(void *arg)
{
    audio_chunk_t chunk;

    while (true) {

        // 🔁 tenta pegar áudio real
        if (xQueueReceive(audio_queue, &chunk, pdMS_TO_TICKS(10))) {

            if (playback_abort) {
                heap_caps_free(chunk.data);
                continue;
            }

            // ===== MONO → STEREO =====
            int16_t *mono = (int16_t *)chunk.data;
            size_t mono_samples = chunk.len / 2;

            size_t stereo_len = mono_samples * 2 * sizeof(int16_t);
            int16_t *stereo = (int16_t *)heap_caps_malloc(
                stereo_len,
                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
            );

            if (!stereo) {
                heap_caps_free(chunk.data);
                continue;
            }

            for (size_t i = 0; i < mono_samples; i++) {
                stereo[2*i]     = mono[i]; // L
                stereo[2*i + 1] = mono[i]; // R
            }

            size_t bytes_written = 0;
            esp_err_t err = i2s_channel_write(
                tx_handle,
                stereo,
                stereo_len,
                &bytes_written,
                portMAX_DELAY
            );

            if (err == ESP_OK) {
                played_bytes_total += bytes_written;
            } else {
                ESP_LOGE(TAG, "i2s write err=%s", esp_err_to_name(err));
            }

            heap_caps_free(stereo);
            heap_caps_free(chunk.data);
        }
        else {
            // 🔇 FILA VAZIA → SILÊNCIO
            size_t w;
            i2s_channel_write(
                tx_handle,
                silence_stereo,
                sizeof(silence_stereo),
                &w,
                portMAX_DELAY
            );
        }
    }
}





// =================== MIC STREAM ===================
static void mic_capture_task(void *arg)
{
    while (true) {
        mic_chunk_t chunk;
        chunk.len = MIC_FRAME_BYTES;

        chunk.pcm = (uint8_t *)heap_caps_malloc(MIC_FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!chunk.pcm) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        esp_err_t err = i2s_channel_read(
            rx_handle,
            chunk.pcm,
            MIC_FRAME_BYTES,
            &chunk.len,
            portMAX_DELAY
        );

        if (err != ESP_OK || chunk.len == 0) {
            heap_caps_free(chunk.pcm);
            continue;
        }

        //bloco detection start
        int16_t *samples = (int16_t *)chunk.pcm;
        size_t sample_count = chunk.len / 2;

        int64_t acc = 0;
        for (size_t i = 0; i < sample_count; i++) {
            int32_t s = samples[i];
            acc += s * s;
        }
        acc /= sample_count;

        // RMS inteiro (sem float pesado)
        //int32_t rms = (int32_t)sqrt((double)acc);
        rms = (int32_t)sqrt((double)acc);

        int64_t now = esp_timer_get_time();


        // ajuste dinâmico dos thresholds
        int DINAM_VAD_THRESHOLD_ON = model_speaking ? VAD_THRESHOLD_ON+VAD_BOOST : VAD_THRESHOLD_ON;
        int DINAM_VAD_THRESHOLD_OFF = model_speaking ? VAD_THRESHOLD_OFF+VAD_BOOST : VAD_THRESHOLD_OFF;

       
        
        #define RMS_HOLD_US   300000  // 300 ms

        if (rms > DINAM_VAD_THRESHOLD_ON) {
            user_speaking = true;
            last_voice_ts = now;
        } else {
            // hold simples para não oscilar
            //if ((now - last_voice_ts) > RMS_HOLD_US) {
                
                user_speaking = false;
            //}
        }

        // static int dbg = 0;
        // if (++dbg % 50 == 0) {
        //     ESP_LOGI(TAG, "MIC RMS=%ld user_speaking=%d", rms, user_speaking);
        // }


        //bloco detection end


        
        if (model_speaking && user_speaking && !cancel_sent_for_response) {

            if (barge_in_mode == BARGE_IN_MANUAL) {
                ESP_LOGW(TAG, "BARGE-IN (MANUAL)");
                on_server_speech_started_interrupt();

            } else {
                ESP_LOGW(TAG, "BARGE-IN (SERVER-HANDLED)");
                // NÃO faz nada aqui
            }
        }

        
        // Sempre enfileira (mic sempre ativo)
        
        if (mic_queue) {
            if (xQueueSend(mic_queue, &chunk, pdMS_TO_TICKS(50)) != pdTRUE) {
                heap_caps_free(chunk.pcm);
            }
        } 
        
        else {
            heap_caps_free(chunk.pcm);
        }
    }
}

static void mic_encode_send_task(void *arg)
{
    mic_chunk_t chunk;

    while (true) {

        if (!ws_connected) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (xQueueReceive(mic_queue, &chunk, portMAX_DELAY)) {

            
            // base64 encode
            size_t b64_len = 0;
            size_t out_max = ((chunk.len + 2) / 3) * 4 + 4;

            char *b64 = (char *)heap_caps_malloc(out_max, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (b64) {
                mbedtls_base64_encode(
                    (unsigned char *)b64,
                    out_max,
                    &b64_len,
                    chunk.pcm,
                    chunk.len
                );
                b64[b64_len] = 0;

                send_ws_audio_append(b64);
                heap_caps_free(b64);
            }

            heap_caps_free(chunk.pcm);
        }
    }
}

// =================== WS EVENT HANDLER ===================
static void ws_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {

        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WS CONNECTED");
            

            ws_connected = true;
            app_set_state(APP_IDLE);
            ws_send_session_update();
            //display
            display_set_state_idle();
            display_set_text("Ready...");
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WS DISCONNECTED");
            ws_connected = false;
            app_set_state(APP_IDLE);
            current_response_id[0] = 0;
            model_speaking = false;
            playback_abort = false;
            break;

        case WEBSOCKET_EVENT_DATA: {
            if (ws_accum_len + data->data_len >= WS_ACCUM_MAX) {
                ESP_LOGE(TAG, "WS buffer overflow, resetando");
                ws_accum_len = 0;
                model_speaking = false;
                
                break;
            }

            memcpy(ws_accum + ws_accum_len, data->data_ptr, data->data_len);
            ws_accum_len += data->data_len;
            ws_accum[ws_accum_len] = 0;

            if (!json_is_complete(ws_accum, ws_accum_len)) break;

            cJSON *root = cJSON_Parse(ws_accum);
            ws_accum_len = 0;

            if (!root) {
                ESP_LOGE(TAG, "JSON inválido");
                model_speaking = false;
                
                break;
            }

            cJSON *type = cJSON_GetObjectItem(root, "type");
            if (cJSON_IsString(type)) {

                // ===== SERVER VAD =====
                if (strcmp(type->valuestring, "input_audio_buffer.speech_started") == 0) {
                    ESP_LOGI(TAG, "SERVER VAD: speech_started");
                    app_set_state(APP_LISTENING);
                    display_set_state_listening();



                }


                else if (strcmp(type->valuestring, "input_audio_buffer.speech_stopped") == 0) {
                    ESP_LOGI(TAG, "SERVER VAD: speech_stopped");
                    app_set_state(APP_THINKING);
                    
                    
                    

                    // 2️⃣ Reset de estado local do mic
                    
                    last_voice_ts = 0;
                }

                else if (strcmp(type->valuestring, "input_audio_buffer.committed") == 0) {
                    ESP_LOGI(TAG, "SERVER VAD: committed");
                    app_set_state(APP_THINKING);

                    
                    if (!cancel_sent_for_response) {

                        ws_send_response_create();  //apenas se desabilitado dentro do parametro do update

                    };
                    
                    

                    // Agora sim: pedir resposta
                    playback_abort = false;          // libera tocar a próxima resposta
                    


                }

                // ===== RESPONSE LIFECYCLE =====
                else if (strcmp(type->valuestring, "response.created") == 0) {
                    cJSON *resp = cJSON_GetObjectItem(root, "response");
                    cJSON *id   = resp ? cJSON_GetObjectItem(resp, "id") : NULL;
                    if (cJSON_IsString(id)) {
                        strncpy(current_response_id, id->valuestring, sizeof(current_response_id) - 1);
                        current_response_id[sizeof(current_response_id) - 1] = 0;
                        ESP_LOGI(TAG, "response.created id=%s", current_response_id);
                    }

                        // 🔑 reset do latch aqui
                    cancel_sent_for_response = false;
                    app_set_state(APP_THINKING);
                }

                // Texto (opcional, só log)
                else if (strcmp(type->valuestring, "response.output_text.delta") == 0) {
                    cJSON *delta = cJSON_GetObjectItem(root, "delta");
                    if (cJSON_IsString(delta)) {
                        ESP_LOGI(TAG, "TEXT: %s", delta->valuestring);
                    }
                }

                // Transcrição do áudio do modelo (opcional, só log)
                else if (strcmp(type->valuestring, "response.audio_transcript.delta") == 0) {
                    cJSON *delta = cJSON_GetObjectItem(root, "delta");
                    if (cJSON_IsString(delta)) {
                        ESP_LOGI(TAG, "ASR: %s", delta->valuestring);
                        display_append_text(delta->valuestring);

                    }
                }

                // ===== AUDIO OUT =====
                else if (strcmp(type->valuestring, "response.audio.delta") == 0) {
                    cJSON *delta = cJSON_GetObjectItem(root, "delta");
                    cJSON *item_id = cJSON_GetObjectItem(root, "item_id");

                    if (cJSON_IsString(item_id)) {
                        strncpy(last_model_item_id, item_id->valuestring, sizeof(last_model_item_id) - 1);
                        last_model_item_id[sizeof(last_model_item_id) - 1] = 0;
                    }

                    if (!audio_item_started) {
                        audio_item_started = true;
                        played_bytes_item_start = played_bytes_total;
                        last_model_content_index = 0;
                    }

                    if (cJSON_IsString(delta)) {
                        model_speaking = true;
                        app_set_state(APP_SPEAKING);
                        display_set_state_speaking();

                        // toca
                        play_audio_base64_pcm16(delta->valuestring);
                    }

                }

                else if (strcmp(type->valuestring, "response.audio.done") == 0) {
                    ESP_LOGI(TAG, "response.audio.done");
                    // ainda pode vir response.done em seguida
                    audio_item_started = false;
                    
                }

                else if (strcmp(type->valuestring, "response.done") == 0) {
                    ESP_LOGI(TAG, "response.done");
                    model_speaking = false;
                    app_set_state(APP_IDLE);
                    display_set_state_idle();
                    current_response_id[0] = 0;
                    audio_item_started = false;
                    cancel_sent_for_response = false; 

                    cJSON *response = cJSON_GetObjectItem(root, "response");
                    if (response && cJSON_IsObject(response)) {

                        cJSON *resp_id = cJSON_GetObjectItem(response, "id");
                        cJSON *status  = cJSON_GetObjectItem(response, "status");

                        if (cJSON_IsString(resp_id) && cJSON_IsString(status)) {
                            ESP_LOGI(TAG, "Response ID: %s | Status: %s",
                            resp_id->valuestring,
                             status->valuestring);
                        }
                    }
                }


                else if (strcmp(type->valuestring, "conversation.item.added") == 0) {
                    cJSON *item_id = cJSON_GetObjectItem(root, "item_id");
                    if (cJSON_IsString(item_id)) {
                        ESP_LOGI(TAG, "conversation.item.added =%s", item_id->valuestring);
                    }
                }

                // ===== Truncate ack =====
                else if (strcmp(type->valuestring, "conversation.item.truncated") == 0) {
                    cJSON *item_id = cJSON_GetObjectItem(root, "item_id");
                    if (cJSON_IsString(item_id)) {
                        ESP_LOGI(TAG, "truncate ACK item_id=%s", item_id->valuestring);
                    }
                }

                // ===== Error =====
                else if (strcmp(type->valuestring, "error") == 0) {
                    cJSON *err = cJSON_GetObjectItem(root, "error");
                    if (err) {
                        char *s = cJSON_PrintUnformatted(err);
                        if (s) {
                            ESP_LOGE(TAG, "WS ERROR: %s", s);
                            model_speaking = false;
                            
                            free(s);
                        }
                    }
                }
            }

            cJSON_Delete(root);
            break;
        }

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WS EVENT ERROR");
            model_speaking = false;
            
            break;

        default:
            break;
    }
}

// =================== WIFI HANDLERS ===================
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WIFI_STA_START -> connect");
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WIFI_DISCONNECTED -> stop WS and reconnect WiFi");

        if (client) {
            esp_websocket_client_stop(client);
        }

        ws_started = false;
        ws_connected = false;
        app_set_state(APP_IDLE);

        esp_wifi_connect();
        return;
    }
}

static void on_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&event->ip_info.ip));

    if (!ws_started) {
        vTaskDelay(pdMS_TO_TICKS(300));
        ESP_LOGI(TAG, "Starting WebSocket...");
        esp_websocket_client_start(client);
        ws_started = true;
    }
}

static void wifi_init(void)
{
    

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {0};
    strcpy((char *)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char *)wifi_config.sta.password, WIFI_PASS);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi init OK");
    
}

// =================== I2S INIT ===================
static void i2s_init_mic_and_pdm_tx(void)
{
    // RX: I2S_NUM_0
    i2s_chan_config_t rx_chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 8,
        .dma_frame_num = 256,
        .auto_clear = true,
    };

    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, &rx_handle));

    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = {
            .sample_rate_hz = AUDIO_SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_PDM_SLOT_LEFT,
        },
        .gpio_cfg = {
            .clk = I2S_PDM_CLK,
            .din = I2S_PDM_DIN,
            .invert_flags = { .clk_inv = false },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_handle, &pdm_rx_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

    
     // ===== TX: I2S STD (MAX98357A) =====
    i2s_chan_config_t tx_chan_cfg = {
        .id = I2S_NUM_1,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
    };

    i2s_new_channel(&tx_chan_cfg, &tx_handle, NULL);

    i2s_std_config_t tx_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(24000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT,
                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_8,
            .ws   = GPIO_NUM_3,
            .dout = GPIO_NUM_43,
            .din  = I2S_GPIO_UNUSED,
        },
    };

    i2s_channel_init_std_mode(tx_handle, &tx_cfg);
    i2s_channel_enable(tx_handle);

    



    ESP_LOGI(TAG, "I2S init OK (RX PDM mic + TX PDM audio)");
}

void boot_beep_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(600)); // WiFi + I2S estáveis

    const int sample_rate = 24000;
    const int freq = 880;          // agradável
    const int duration_ms = 60;
    const int amplitude = 2800;

    int samples = (sample_rate * duration_ms) / 1000;

    int16_t *stereo = (int16_t *)heap_caps_malloc(
        samples * 2 * sizeof(int16_t),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
    );
    if (!stereo) vTaskDelete(NULL);

    for (int i = 0; i < samples; i++) {
        float t = (float)i / sample_rate;
        int16_t s = (int16_t)(amplitude * sinf(2.0f * M_PI * freq * t));
        stereo[2*i]     = s;
        stereo[2*i + 1] = s;
    }

    size_t written;
    i2s_channel_write(
        tx_handle,
        stereo,
        samples * 2 * sizeof(int16_t),
        &written,
        portMAX_DELAY
    );

    heap_caps_free(stereo);

    // 🛑 PASSO CRÍTICO — SILÊNCIO FORÇADO
    int16_t silence[256] = {0};
    for (int i = 0; i < 6; i++) {
        i2s_channel_write(tx_handle, silence, sizeof(silence), &written, portMAX_DELAY);
    }

    // 🧠 garante que nenhuma task ficou escrevendo
    vTaskDelay(pdMS_TO_TICKS(40));

    // 🔥 MORRE DE VERDADE
    vTaskDelete(NULL);
}






// =================== APP MAIN ===================
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "BOOT OK");



    // NVS
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(nvs_ret);
    }

    // Eye animation (PRINCIPAL - inicializa display)
    eye_animation_init();
    
    // Display (SECUNDÁRIO - agora é apenas API)
    display_init();

    eye_set_text("Iniciando...");

    

    // Wi-Fi
    wifi_init();

    // WebSocket
    esp_websocket_client_config_t ws_cfg = {0};
    ws_cfg.uri = REALTIME_URI;
    ws_cfg.buffer_size = 16384;
    ws_cfg.reconnect_timeout_ms = 10000;
    ws_cfg.network_timeout_ms = 10000;
    ws_cfg.ping_interval_sec = 15;
    ws_cfg.transport = WEBSOCKET_TRANSPORT_OVER_SSL;
    ws_cfg.disable_auto_reconnect = false;
    ws_cfg.crt_bundle_attach = esp_crt_bundle_attach;

    ws_cfg.headers =
        "Authorization: Bearer " OPENAI_KEY "\r\n"
        "OpenAI-Beta: realtime=v1\r\n";

    client = esp_websocket_client_init(&ws_cfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL);

    // I2S
    i2s_init_mic_and_pdm_tx();

    // Queues
    mic_queue = xQueueCreate(MIC_QUEUE_LEN, sizeof(mic_chunk_t));
    audio_queue = xQueueCreate(AUDIO_QUEUE_LEN, sizeof(audio_chunk_t));

    // Tasks
    xTaskCreatePinnedToCore(mic_capture_task, "mic_capture_task", 4096, NULL, 6, NULL, 1);
    xTaskCreatePinnedToCore(mic_encode_send_task, "mic_send_task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(audio_play_task, "audio_play_task", 8192, NULL, 6, NULL, 1);

    //xTaskCreatePinnedToCore(servo_task,"servo_task",4096, NULL, 2, NULL, 1 );
   
    xTaskCreatePinnedToCore(
        boot_beep_task,
        "boot_beep",
        4096,
        NULL,
        4,
        NULL,
        1
    );



        
    // Loop
    while (true) {
        // Só pra você ver o estado vivo
        ESP_LOGI(TAG, "alive | ws=%d | state=%d | model_speaking=%d | user_speaking=%d -> RMS=%ld ", ws_connected, (int)app_state, model_speaking, user_speaking, rms);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}
