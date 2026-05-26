#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "config.h"
#include "beacon.h"
#include "web.h"

static httpd_handle_t server = NULL;
static wifi_ap_record_t scan_records[30];
static uint16_t scan_count = 0;

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
        for (int i = 0; i < beacon_get_ssid_count(); i++) {
            char ssid[33];
            uint8_t ch;
            if (beacon_get_ssid_at(i, ssid, &ch)) {
                if (i > 0) pos += sprintf(resp + pos, ",");
                pos += sprintf(resp + pos, "{\"ssid\":\"%s\",\"channel\":%d}", ssid, ch);
            }
        }
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
            if (ssid) {
                ssid += 8;
                char *end = strchr(ssid, '"');
                if (end) {
                    int slen = end - ssid;
                    if (slen > 0 && slen <= 32) {
                        char ssid_copy[33];
                        strncpy(ssid_copy, ssid, slen);
                        ssid_copy[slen] = 0;
                        uint8_t channel = 6;
                        if (ch) channel = atoi(ch + 10);
                        beacon_add_ssid(ssid_copy, channel);
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
            beacon_remove_ssid(i);
        }
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":true}");
    }
    return ESP_OK;
}

static esp_err_t api_spoof_start_handler(httpd_req_t *req)
{
    printf("API: spoof start\n");
    beacon_start();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t api_spoof_stop_handler(httpd_req_t *req)
{
    printf("API: spoof stop\n");
    beacon_stop();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t api_status_handler(httpd_req_t *req)
{
    char resp[64];
    bool active = beacon_is_active();
    uint8_t ch = beacon_get_channel();
    int count = beacon_get_ssid_count();
    sprintf(resp, "{\"active\":%s,\"channel\":%d,\"count\":%d}", active ? "true" : "false", ch, count);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

void start_webserver(void)
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
