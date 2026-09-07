#include "device_config.hpp"
#include "device_config_validation.hpp"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "nvs.h"

namespace {
constexpr const char *TAG = "DEVICE_CONFIG";
constexpr const char *NVS_NAMESPACE = "chatbot_cfg";
constexpr const char *NVS_WIFI_SSID = "wifi_ssid";
constexpr const char *NVS_WIFI_PASSWORD = "wifi_pass";
constexpr const char *NVS_OPENAI_KEY = "openai_key";
constexpr int PORTAL_MAX_BODY_SIZE = 512;

httpd_handle_t s_http_server = nullptr;
esp_netif_t *s_ap_netif = nullptr;
TaskHandle_t s_dns_task = nullptr;
volatile bool s_dns_running = false;
char s_portal_ssid[33] = {};
char s_portal_password[16] = {};
device_config_status_callback_t s_status_callback = nullptr;
esp_timer_handle_t s_restart_timer = nullptr;

const char PORTAL_HTML[] = R"HTML(<!doctype html><html lang="pt-BR"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ESP32 Realtime</title><style>body{font-family:Arial,sans-serif;max-width:32rem;margin:2rem auto;padding:0 1rem;color:#172033}label{display:block;font-weight:600;margin-top:1rem}input{box-sizing:border-box;width:100%;padding:.75rem;margin-top:.35rem;border:1px solid #aab4c5;border-radius:.4rem}button{margin-top:1.5rem;padding:.8rem 1rem;width:100%;border:0;border-radius:.4rem;background:#146cdd;color:#fff;font-weight:700}p{line-height:1.4;color:#526070}.error{display:none;margin-top:1rem;padding:.75rem;border-radius:.4rem;background:#fee2e2;color:#991b1b}.ok{margin-top:1rem;color:#166534}</style><h1>Configurar ESP32</h1><p>Informe a rede Wi-Fi e a chave da API. A chave não será exibida novamente.</p><form id="config-form"><label>Nome da rede Wi-Fi (SSID)<input name="ssid" maxlength="32" required autocomplete="off"></label><label>Senha Wi-Fi<input name="password" type="password" maxlength="64" autocomplete="new-password"></label><label>OpenAI API key<input name="api_key" type="password" maxlength="255" required autocomplete="new-password"></label><button id="save" type="submit">Salvar e reiniciar</button></form><p id="message" class="error" role="alert"></p><script>const f=document.getElementById('config-form'),m=document.getElementById('message'),b=document.getElementById('save');f.addEventListener('submit',async e=>{e.preventDefault();m.style.display='none';b.disabled=true;try{const r=await fetch('/api/config',{method:'POST',body:new URLSearchParams(new FormData(f))});const d=await r.json();if(!r.ok)throw Error(d.error||'Não foi possível salvar.');m.className='ok';m.textContent='Dados salvos. O dispositivo será reiniciado.';m.style.display='block';}catch(e){m.className='error';m.textContent=e.message;m.style.display='block';b.disabled=false;}});</script></html>)HTML";

void notify_status(const char *message) {
    ESP_LOGI(TAG, "%s", message);
    if (s_status_callback != nullptr) {
        s_status_callback(message);
    }
}

bool nvs_get_string(nvs_handle_t handle, const char *key, char *out, size_t out_size) {
    size_t required_size = out_size;
    esp_err_t err = nvs_get_str(handle, key, out, &required_size);
    return err == ESP_OK && required_size <= out_size;
}

void url_decode(char *value) {
    char *read = value;
    char *write = value;
    while (*read != '\0') {
        if (*read == '+' ) {
            *write++ = ' ';
            ++read;
        } else if (*read == '%' && isxdigit(static_cast<unsigned char>(read[1])) && isxdigit(static_cast<unsigned char>(read[2]))) {
            char hex[3] = {read[1], read[2], '\0'};
            *write++ = static_cast<char>(strtol(hex, nullptr, 16));
            read += 3;
        } else {
            *write++ = *read++;
        }
    }
    *write = '\0';
}

enum class form_result_t {
    ok,
    missing_ssid,
    missing_password,
    missing_api_key,
    field_too_long,
};

bool copy_form_value(const char *value, char *out, size_t out_size) {
    const size_t value_len = strlen(value);
    if (value_len >= out_size) return false;
    memcpy(out, value, value_len + 1);
    return true;
}

form_result_t parse_config_form(char *body, device_config_t *config) {
    bool have_ssid = false;
    bool have_password = false;
    bool have_api_key = false;
    for (char *pair = strtok(body, "&"); pair != nullptr; pair = strtok(nullptr, "&")) {
        char *separator = strchr(pair, '=');
        if (separator == nullptr) continue;
        *separator = '\0';
        char *value = separator + 1;
        url_decode(value);
        bool copied = true;
        if (strcmp(pair, "ssid") == 0) {
            have_ssid = true;
            copied = copy_form_value(value, config->wifi_ssid, sizeof(config->wifi_ssid));
        } else if (strcmp(pair, "password") == 0) {
            have_password = true;
            copied = copy_form_value(value, config->wifi_password, sizeof(config->wifi_password));
        } else if (strcmp(pair, "api_key") == 0) {
            have_api_key = true;
            copied = copy_form_value(value, config->openai_api_key, sizeof(config->openai_api_key));
        }
        if (!copied) return form_result_t::field_too_long;
    }
    if (!have_ssid || config->wifi_ssid[0] == '\0') return form_result_t::missing_ssid;
    if (!have_password) return form_result_t::missing_password;
    if (!have_api_key || config->openai_api_key[0] == '\0') return form_result_t::missing_api_key;
    return form_result_t::ok;
}

esp_err_t json_error(httpd_req_t *request, const char *status, const char *message) {
    httpd_resp_set_status(request, status);
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, message);
}

void restart_timer_callback(void *) {
    esp_restart();
}

esp_err_t root_get_handler(httpd_req_t *request) {
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, PORTAL_HTML, HTTPD_RESP_USE_STRLEN);
}

esp_err_t config_post_handler(httpd_req_t *request) {
    if (request->content_len <= 0 || request->content_len >= PORTAL_MAX_BODY_SIZE) {
        json_error(request, "400 Bad Request", "{\"error\":\"Os dados enviados são inválidos ou grandes demais.\"}");
        return ESP_FAIL;
    }

    char body[PORTAL_MAX_BODY_SIZE] = {};
    int received = httpd_req_recv(request, body, request->content_len);
    if (received != request->content_len) {
        json_error(request, "400 Bad Request", "{\"error\":\"Não foi possível receber todos os dados. Tente novamente.\"}");
        return ESP_FAIL;
    }
    body[received] = '\0';

    device_config_t config = {};
    form_result_t form_result = parse_config_form(body, &config);
    if (form_result != form_result_t::ok) {
        const char *error = "{\"error\":\"Dados inválidos. Revise os campos.\"}";
        if (form_result == form_result_t::missing_ssid) error = "{\"error\":\"Informe o nome da rede Wi-Fi.\"}";
        if (form_result == form_result_t::missing_password) error = "{\"error\":\"Informe a senha Wi-Fi ou deixe o campo vazio para uma rede aberta.\"}";
        if (form_result == form_result_t::missing_api_key) error = "{\"error\":\"A API key é obrigatória.\"}";
        if (form_result == form_result_t::field_too_long) error = "{\"error\":\"Um dos campos excede o tamanho permitido.\"}";
        json_error(request, "400 Bad Request", error);
        return ESP_FAIL;
    }

    esp_err_t err = device_config_save(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao salvar configuracao: %s", esp_err_to_name(err));
        json_error(request, "500 Internal Server Error", "{\"error\":\"Não foi possível salvar a configuração. Tente novamente.\"}");
        return err;
    }

    httpd_resp_set_type(request, "application/json");
    httpd_resp_sendstr(request, "{\"ok\":true,\"restarting\":true}");
    notify_status("Configuracao salva. Reiniciando...");
    esp_timer_start_once(s_restart_timer, 1000 * 1000);
    return ESP_OK;
}

void dns_server_task(void *) {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (socket_fd < 0) {
        ESP_LOGE(TAG, "Nao foi possivel abrir DNS");
        vTaskDelete(nullptr);
        return;
    }

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(53);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(socket_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        ESP_LOGE(TAG, "Nao foi possivel associar DNS");
        close(socket_fd);
        vTaskDelete(nullptr);
        return;
    }

    uint8_t request[256];
    uint8_t response[272];
    while (s_dns_running) {
        sockaddr_in source = {};
        socklen_t source_len = sizeof(source);
        int length = recvfrom(socket_fd, request, sizeof(request), 0, reinterpret_cast<sockaddr *>(&source), &source_len);
        if (length < 12) {
            continue;
        }
        int question_end = 12;
        while (question_end < length && request[question_end] != 0) {
            question_end += request[question_end] + 1;
        }
        question_end += 5; // zero label + QTYPE + QCLASS
        if (question_end > length || question_end + 16 > static_cast<int>(sizeof(response))) {
            continue;
        }

        memcpy(response, request, 2);
        response[2] = 0x81;
        response[3] = 0x80;
        response[4] = 0x00; response[5] = 0x01;
        response[6] = 0x00; response[7] = 0x01;
        response[8] = response[9] = response[10] = response[11] = 0x00;
        memcpy(response + 12, request + 12, question_end - 12);
        int offset = question_end;
        response[offset++] = 0xC0; response[offset++] = 0x0C;
        response[offset++] = 0x00; response[offset++] = 0x01;
        response[offset++] = 0x00; response[offset++] = 0x01;
        response[offset++] = 0x00; response[offset++] = 0x00;
        response[offset++] = 0x00; response[offset++] = 0x3C;
        response[offset++] = 0x00; response[offset++] = 0x04;
        response[offset++] = 192; response[offset++] = 168; response[offset++] = 4; response[offset++] = 1;
        sendto(socket_fd, response, offset, 0, reinterpret_cast<sockaddr *>(&source), source_len);
    }

    close(socket_fd);
    s_dns_task = nullptr;
    vTaskDelete(nullptr);
}
} // namespace

bool device_config_is_valid(const device_config_t *config) {
    return config != nullptr && device_config_fields_valid(config->wifi_ssid, config->wifi_password, config->openai_api_key);
}

bool device_config_load(device_config_t *config) {
    if (config == nullptr) {
        return false;
    }
    *config = {};
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    bool loaded = nvs_get_string(handle, NVS_WIFI_SSID, config->wifi_ssid, sizeof(config->wifi_ssid)) &&
                  nvs_get_string(handle, NVS_WIFI_PASSWORD, config->wifi_password, sizeof(config->wifi_password)) &&
                  nvs_get_string(handle, NVS_OPENAI_KEY, config->openai_api_key, sizeof(config->openai_api_key));
    nvs_close(handle);
    return loaded && device_config_is_valid(config);
}

esp_err_t device_config_save(const device_config_t *config) {
    if (!device_config_is_valid(config)) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(handle, NVS_WIFI_SSID, config->wifi_ssid);
    if (err == ESP_OK) err = nvs_set_str(handle, NVS_WIFI_PASSWORD, config->wifi_password);
    if (err == ESP_OK) err = nvs_set_str(handle, NVS_OPENAI_KEY, config->openai_api_key);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t device_config_erase(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_erase_all(handle);
        if (err == ESP_OK) err = nvs_commit(handle);
        nvs_close(handle);
    }
    return err;
}

const char *device_config_portal_ssid(void) {
    return s_portal_ssid;
}

const char *device_config_portal_password(void) {
    return s_portal_password;
}

esp_err_t device_config_start_portal(device_config_status_callback_t status_callback) {
    if (s_http_server != nullptr) {
        return ESP_OK;
    }
    s_status_callback = status_callback;

    strlcpy(s_portal_ssid, "esp32s3", sizeof(s_portal_ssid));
    strlcpy(s_portal_password, "art123@#", sizeof(s_portal_password));

    if (s_ap_netif == nullptr) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
    }
    wifi_config_t ap_config = {};
    strlcpy(reinterpret_cast<char *>(ap_config.ap.ssid), s_portal_ssid, sizeof(ap_config.ap.ssid));
    strlcpy(reinterpret_cast<char *>(ap_config.ap.password), s_portal_password, sizeof(ap_config.ap.password));
    ap_config.ap.ssid_len = strlen(s_portal_ssid);
    ap_config.ap.channel = 1;
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap_config.ap.max_connection = 4;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA), TAG, "Falha ao ativar AP");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_config), TAG, "Falha ao configurar AP");
    esp_err_t start_err = esp_wifi_start();
    if (start_err != ESP_OK && start_err != ESP_ERR_WIFI_CONN) {
        ESP_RETURN_ON_ERROR(start_err, TAG, "Falha ao iniciar Wi-Fi");
    }

    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.uri_match_fn = httpd_uri_match_wildcard;
    ESP_RETURN_ON_ERROR(httpd_start(&s_http_server, &server_config), TAG, "Falha ao iniciar HTTP");
    httpd_uri_t root = {.uri = "/*", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = nullptr};
    httpd_uri_t post = {.uri = "/api/config", .method = HTTP_POST, .handler = config_post_handler, .user_ctx = nullptr};
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &root), TAG, "Falha ao registrar GET");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &post), TAG, "Falha ao registrar POST");

    if (s_restart_timer == nullptr) {
        esp_timer_create_args_t timer_args = {.callback = restart_timer_callback, .arg = nullptr, .dispatch_method = ESP_TIMER_TASK, .name = "portal_restart", .skip_unhandled_events = true};
        ESP_RETURN_ON_ERROR(esp_timer_create(&timer_args, &s_restart_timer), TAG, "Falha ao criar timer");
    }

    s_dns_running = true;
    xTaskCreate(dns_server_task, "portal_dns", 3072, nullptr, 4, &s_dns_task);
    notify_status("Modo configuracao: conecte ao Wi-Fi do dispositivo e abra 192.168.4.1");
    ESP_LOGI(TAG, "Portal iniciado: SSID %s, IP %s", s_portal_ssid, DEVICE_CONFIG_PORTAL_IP);
    return ESP_OK;
}

void device_config_stop_portal(void) {
    s_dns_running = false;
    if (s_http_server != nullptr) {
        httpd_stop(s_http_server);
        s_http_server = nullptr;
    }
}
