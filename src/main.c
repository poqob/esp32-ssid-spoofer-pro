#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"

#define AP_SSID     "saturn"
#define AP_PASS     "kova3210"
#define AP_CHANNEL  6
#define MAX_SSID    20
#define BEACON_BURST 3
#define BEACON_INTERVAL_MS 10

static char selected_ssids[MAX_SSID][33];
static int selected_count = 0;
static bool spoof_active = false;
static int spoof_channel = AP_CHANNEL;
static SemaphoreHandle_t mutex = NULL;
static httpd_handle_t server = NULL;
static wifi_ap_record_t scan_records[30];
static uint16_t scan_count = 0;

static int build_beacon_frame(uint8_t *frame, const char *ssid, uint8_t idx, uint8_t channel)
{
    int pos = 0;
    int ssid_len = strlen(ssid);
    if (ssid_len > 32) ssid_len = 32;

    frame[pos++] = 0x80;
    frame[pos++] = 0x00;
    frame[pos++] = 0x00;
    frame[pos++] = 0x00;
    memset(&frame[pos], 0xFF, 6);
    pos += 6;
    frame[pos++] = 0xDE;
    frame[pos++] = 0xAD;
    frame[pos++] = 0xBE;
    frame[pos++] = 0xEF;
    frame[pos++] = idx;
    frame[pos++] = 0x00;
    memcpy(&frame[pos], &frame[pos - 6], 6);
    pos += 6;
    frame[pos++] = 0x00;
    frame[pos++] = 0x00;
    memset(&frame[pos], 0, 8);
    pos += 8;
    frame[pos++] = 0x64;
    frame[pos++] = 0x00;
    frame[pos++] = 0x21;
    frame[pos++] = 0x04;

    frame[pos++] = 0x00;
    frame[pos++] = ssid_len;
    memcpy(&frame[pos], ssid, ssid_len);
    pos += ssid_len;

    static const uint8_t rates[] = {0x82, 0x84, 0x8B, 0x96, 0x0C, 0x12, 0x18, 0x24};
    frame[pos++] = 0x01;
    frame[pos++] = sizeof(rates);
    memcpy(&frame[pos], rates, sizeof(rates));
    pos += sizeof(rates);

    frame[pos++] = 0x03;
    frame[pos++] = 0x01;
    frame[pos++] = channel;

    frame[pos++] = 0x2A;
    frame[pos++] = 0x01;
    frame[pos++] = 0x00;

    static const uint8_t erates[] = {0x30, 0x48, 0x60, 0x6C};
    frame[pos++] = 0x32;
    frame[pos++] = sizeof(erates);
    memcpy(&frame[pos], erates, sizeof(erates));
    pos += sizeof(erates);

    return pos;
}

static void spoof_task(void *arg)
{
    uint8_t *frame = malloc(512);
    if (!frame) {
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        bool active = spoof_active;
        int ch = spoof_channel;
        char ssids[MAX_SSID][33];
        int count = selected_count;
        for (int i = 0; i < count; i++) {
            strcpy(ssids[i], selected_ssids[i]);
        }
        xSemaphoreGive(mutex);

        if (!active || count == 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);

        for (int s = 0; s < count; s++) {
            for (int b = 0; b < BEACON_BURST; b++) {
                int len = build_beacon_frame(frame, ssids[s], s, ch);
                esp_wifi_80211_tx(WIFI_IF_AP, frame, len, false);
            }
            vTaskDelay(pdMS_TO_TICKS(BEACON_INTERVAL_MS));
        }
    }
    free(frame);
}

static void wifi_init(void)
{
    printf("WiFi init...\n");
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    printf("WiFi init OK\n");

    wifi_config_t ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .password = AP_PASS,
            .channel = AP_CHANNEL,
            .max_connection = 10,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    printf("Setting mode...\n");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    printf("Setting config...\n");
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    printf("Starting WiFi...\n");
    ESP_ERROR_CHECK(esp_wifi_start());
    printf("AP: %s (pass: %s)\n", AP_SSID, AP_PASS);
}

static const char INDEX_HTML[] = 
    "<!DOCTYPE html><html><head>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<style>body{font-family:monospace;background:#1a1a2e;color:#0f0;padding:15px}"
    "button{background:#222;color:#0f0;border:1px solid #0f0;padding:8px;margin:5px;cursor:pointer}"
    ".s{background:#222;padding:10px;margin:10px 0;border:1px solid #0f0}"
    ".i{background:#111;padding:8px;margin:4px 0}</style>"
    "</head><body><h1>Saturn Spoofer</h1>"
    "<div id=s>Kapali</div><div class=s><button onclick=scan()>TARA</button></div>"
    "<div class=s><h3>Aglar</h3><div id=n>Tara butonuna bas</div></div>"
    "<div class=s><h3>Secili</h3><div id=l></div><input id=c placeholder=SSID><button onclick=ac()>EKLE</button></div>"
    "<div class=s><button onclick=sf()>BASLAT</button><button onclick=sp()>DURDUR</button></div>"
    "<script>"
    "async function scan(){document.getElementById('n').innerHTML='TARANIYOR...';"
    "try{let r=await fetch('/api/scan');let d=await r.json();console.log('scan:',d);"
    "if(d.error){document.getElementById('n').innerHTML='HATA:'+d.error;return;}"
    "let h='';if(!d.aps||d.aps.length===0){document.getElementById('n').innerHTML='Ag yok';return;}"
    "d.aps.forEach((x,i)=>{h+='<div class=i>'+x.ssid+' CH:'+x.ch+' <button onclick=as(\\''+x.ssid+'\\','+x.ch+')>+</button></div>';});"
    "document.getElementById('n').innerHTML=h;}catch(e){document.getElementById('n').innerHTML='HATA:'+e;}}"
    "async function as(s,c){await fetch('/api/select',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s,channel:c})});ld();}"
    "async function ac(){let s=document.getElementById('c').value;if(s){await fetch('/api/select',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s,channel:6})});document.getElementById('c').value='';ld();}}"
    "async function rm(i){await fetch('/api/select/delete?i='+i,{method:'DELETE'});ld();}"
    "async function ld(){try{let r=await fetch('/api/select');let d=await r.json();let h='';"
    "d.ssids.forEach((s,i)=>{h+='<div class=i>'+s.ssid+' <button onclick=rm('+i+')>-</button></div>';});"
    "document.getElementById('l').innerHTML=h||'Yok';}catch(e){console.log(e);}}"
    "async function sf(){await fetch('/api/spoof/start',{method:'POST'});up();}"
    "async function sp(){await fetch('/api/spoof/stop',{method:'POST'});up();}"
    "async function up(){try{let r=await fetch('/api/status');let d=await r.json();"
    "document.getElementById('s').textContent=d.active?'AKTIF CH:'+d.channel:'KAPALI';}catch(e){}}"
    "setInterval(up,2000);ld();"
    "</script></body></html>";

static esp_err_t index_handler(httpd_req_t *req)
{
    printf("HTTP GET /\n");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, INDEX_HTML, sizeof(INDEX_HTML) - 1);
    return ESP_OK;
}

static esp_err_t api_scan_handler(httpd_req_t *req)
{
    printf("=== SCAN START ===\n");
    fflush(stdout);
    
    esp_err_t ret = esp_wifi_scan_start(NULL, true);
    printf("SCAN: start ret=%d\n", ret);
    fflush(stdout);
    
    if (ret != ESP_OK) {
        printf("SCAN: failed!\n");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\":\"scan failed\"}");
        return ESP_OK;
    }
    
    scan_count = 30;
    ret = esp_wifi_scan_get_ap_records(&scan_count, scan_records);
    printf("SCAN: get ret=%d count=%d\n", ret, scan_count);
    fflush(stdout);

    char *resp = malloc(16384);
    if (!resp) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    int pos = 0;
    pos += sprintf(resp, "{\"aps\":[");
    for (int i = 0; i < scan_count && i < 30; i++) {
        if (i > 0) pos += sprintf(resp + pos, ",");
        char ssid_esc[65] = {0};
        int j = 0;
        for (int k = 0; k < 32 && scan_records[i].ssid[k]; k++) {
            if (scan_records[i].ssid[k] == '"' || scan_records[i].ssid[k] == '\\') ssid_esc[j++] = '\\';
            ssid_esc[j++] = scan_records[i].ssid[k];
        }
        pos += sprintf(resp + pos, "{\"ssid\":\"%s\",\"ch\":%d}", ssid_esc, scan_records[i].primary);
        printf("AP[%d]: %s CH=%d\n", i, ssid_esc, scan_records[i].primary);
    }
    pos += sprintf(resp + pos, "]}");
    printf("=== SCAN DONE len=%d ===\n", pos);
    fflush(stdout);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, pos);
    free(resp);
    return ESP_OK;
}

static esp_err_t api_select_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        char *resp = malloc(4096);
        int pos = 0;
        pos += sprintf(resp, "{\"ssids\":[");
        xSemaphoreTake(mutex, portMAX_DELAY);
        for (int i = 0; i < selected_count; i++) {
            if (i > 0) pos += sprintf(resp + pos, ",");
            pos += sprintf(resp + pos, "{\"ssid\":\"%s\",\"channel\":%d}", selected_ssids[i], spoof_channel);
        }
        xSemaphoreGive(mutex);
        pos += sprintf(resp + pos, "]}");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, pos);
        free(resp);
    } else if (req->method == HTTP_POST) {
        char buf[128];
        int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = 0;
            char *ssid = strstr(buf, "\"ssid\":\"");
            char *ch = strstr(buf, "\"channel\":");
            if (ssid && selected_count < MAX_SSID) {
                ssid += 8;
                char *end = strchr(ssid, '"');
                if (end) {
                    int slen = end - ssid;
                    if (slen > 0 && slen <= 32) {
                        xSemaphoreTake(mutex, portMAX_DELAY);
                        strncpy(selected_ssids[selected_count], ssid, slen);
                        selected_ssids[selected_count][slen] = 0;
                        if (ch) spoof_channel = atoi(ch + 10);
                        selected_count++;
                        xSemaphoreGive(mutex);
                        printf("Added SSID: %s CH:%d\n", selected_ssids[selected_count-1], spoof_channel);
                    }
                }
            }
        }
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":true}");
    } else if (req->method == HTTP_DELETE) {
        char *uri = (char *)req->uri;
        printf("API: DELETE %s\n", uri);
        char *qs = strstr(uri, "?i=");
        if (qs) {
            int i = atoi(qs + 3);
            printf("API: delete index %d\n", i);
            xSemaphoreTake(mutex, portMAX_DELAY);
            if (i >= 0 && i < selected_count) {
                for (int j = i; j < selected_count - 1; j++) {
                    strcpy(selected_ssids[j], selected_ssids[j+1]);
                }
                selected_ssids[selected_count-1][0] = 0;
                selected_count--;
                printf("API: deleted, now %d SSIDs\n", selected_count);
            }
            xSemaphoreGive(mutex);
        }
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":true}");
    }
    return ESP_OK;
}

static esp_err_t api_spoof_start_handler(httpd_req_t *req)
{
    printf("API: spoof start\n");
    xSemaphoreTake(mutex, portMAX_DELAY);
    spoof_active = true;
    xSemaphoreGive(mutex);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t api_spoof_stop_handler(httpd_req_t *req)
{
    printf("API: spoof stop\n");
    xSemaphoreTake(mutex, portMAX_DELAY);
    spoof_active = false;
    esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE);
    xSemaphoreGive(mutex);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t api_status_handler(httpd_req_t *req)
{
    char resp[64];
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool active = spoof_active;
    int ch = spoof_channel;
    int count = selected_count;
    xSemaphoreGive(mutex);
    sprintf(resp, "{\"active\":%s,\"channel\":%d,\"count\":%d}", active ? "true" : "false", ch, count);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.max_uri_handlers = 16;

    printf("Starting HTTP server...\n");
    if (httpd_start(&server, &config) != ESP_OK) {
        printf("HTTP FAILED\n");
        return;
    }
    printf("HTTP server started\n");

    httpd_uri_t uris[] = {
        {"/", HTTP_GET, index_handler, NULL},
        {"/api/scan", HTTP_GET, api_scan_handler, NULL},
        {"/api/select", HTTP_GET, api_select_handler, NULL},
        {"/api/select", HTTP_POST, api_select_handler, NULL},
        {"/api/select/delete", HTTP_DELETE, api_select_handler, NULL},
        {"/api/spoof/start", HTTP_POST, api_spoof_start_handler, NULL},
        {"/api/spoof/stop", HTTP_POST, api_spoof_stop_handler, NULL},
        {"/api/status", HTTP_GET, api_status_handler, NULL},
    };

    for (int i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(server, &uris[i]);
    }

    printf("HTTP: 192.168.4.1\n");
}

void app_main(void)
{
    printf("Saturn SSID Spoofer starting\n");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        printf("NVS init failed: %d\n", ret);
        return;
    }

    esp_netif_init();
    esp_event_loop_create_default();
    mutex = xSemaphoreCreateMutex();

    wifi_init();
    start_webserver();

    xTaskCreate(spoof_task, "spoof", 4096, NULL, 5, NULL);

    printf("Ready! Connect to '%s' and visit http://192.168.4.1\n", AP_SSID);
}
