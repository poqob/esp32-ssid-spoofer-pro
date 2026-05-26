#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
    "<title>Saturn Spoofer</title>"
    "<style>:root{--bg:#000;--text:#ddd;--card:#111;--border:#222;--muted:#666;--accent:#4fc3f7;--green:#66bb6a;--red:#ef5350;--orange:#ffa726;--blue:#42a5f5;--btn:#1a1a1a;--btn-hover:#2a2a2a;--input:#1a1a1a}"
    "[data-theme=light]{--bg:#fff;--text:#333;--card:#f5f5f5;--border:#e0e0e0;--muted:#999;--accent:#0288d1;--green:#43a047;--red:#e53935;--orange:#ef6c00;--blue:#1565c0;--btn:#eee;--btn-hover:#ddd;--input:#fff}"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',system-ui,sans-serif;background:var(--bg);color:var(--text);padding:16px;font-size:14px;line-height:1.5;-webkit-font-smoothing:antialiased}"
    ".top{display:flex;align-items:center;justify-content:space-between;margin-bottom:16px}"
    "h1{font-size:18px;font-weight:700}"
    "#tt{background:none;border:1px solid var(--border);border-radius:6px;padding:4px 10px;cursor:pointer;color:var(--text);font-size:16px;line-height:1}"
    "#tt:hover{border-color:var(--accent)}"
    ".card{border:1px solid var(--border);border-radius:8px;padding:12px;margin-bottom:10px}"
    ".card h3{font-size:13px;font-weight:600;margin-bottom:8px;text-transform:uppercase;letter-spacing:.5px;color:var(--muted)}"
    "button{background:var(--btn);color:var(--text);border:1px solid var(--border);border-radius:6px;padding:6px 12px;cursor:pointer;font-size:13px;transition:.15s}"
    "button:hover{background:var(--btn-hover);border-color:var(--accent)}"
    ".row{display:flex;flex-wrap:wrap;gap:6px;align-items:center}"
    ".mt6{margin-top:6px}"
    ".mb6{margin-bottom:6px}"
    ".gap{gap:8px}"
    "input{background:var(--input);color:var(--text);border:1px solid var(--border);border-radius:6px;padding:6px 10px;font-size:13px;width:160px;outline:none}"
    "input:focus{border-color:var(--accent)}"
    "#s{font-size:13px;color:var(--muted)}"
    "#s.ok{color:var(--green)}"
    "#s.off{color:var(--muted)}"
    ".e{border-bottom:1px solid var(--border);padding:8px 0;font-size:13px}"
    ".e:last-child{border:0}"
    ".e .n{font-weight:600}"
    ".e .m{font-family:SFMono,monospace;font-size:12px;color:var(--accent)}"
    ".tag{color:var(--muted);font-size:11px;margin-left:4px}"
    ".tag-g{color:var(--green)}"
    ".tag-r{color:var(--red)}"
    ".tag-o{color:var(--orange)}"
    ".tag-b{color:var(--blue)}"
    ".sc,.sl{font-size:12px;color:var(--muted);padding:8px 0}"
    "#lg{max-height:280px;overflow-y:auto}"
    ".lh{font-size:11px;color:var(--muted);padding:4px 0;display:flex;gap:8px;border-bottom:1px solid var(--border)}"
    ".lh span{flex-shrink:0}"
    ".w{color:var(--muted);padding:8px 0;font-size:12px}"
    "</style></head><body>"
    "<div class=top><h1>Saturn Spoofer</h1><button id=tt onclick=tm()>&#x2601;&#xFE0F;</button></div>"
    "<div class=card><div id=s class=off>&#x25CF; Idle</div></div>"
    "<div class='card row gap'>"
    "<button onclick=sf()>Start</button>"
    "<button onclick=sp()>Stop</button>"
    "</div>"
    "<div class=card><h3>Scan</h3>"
    "<button onclick=scan()>Scan</button>"
    "<div id=n class=w>Press scan</div></div>"
    "<div class=card><h3>Selected SSIDs</h3>"
    "<div class='row gap mb6'><input id=c placeholder='New SSID'><button onclick=ac()>Add</button></div>"
    "<div id=l></div>"
    "<div class=w>Pass: 0174658631</div></div>"
    "<div class=card><h3>Log</h3>"
    "<div id=lg><div class=sc>Waiting...</div></div>"
    "<button onclick=rl() style=margin-top:6px>Reset</button></div>"
    "<script>"
    "(function(){let t=localStorage.getItem('t');if(t)document.documentElement.setAttribute('data-theme',t);"
    "let b=document.getElementById('tt');b.textContent='\\u2601\\uFE0F'})();"
    "function tm(){let d=document.documentElement;let c=d.getAttribute('data-theme');let n=c==='light'?'':'light';"
    "d.setAttribute('data-theme',n);localStorage.setItem('t',n);"
    "document.getElementById('tt').textContent=n==='light'?'\\u2600\\uFE0F':'\\u2601\\uFE0F'}"
    "async function scan(){document.getElementById('n').innerHTML='<span>Scanning...</span>';"
    "try{let r=await fetch('/api/scan');let d=await r.json();"
    "if(d.error){document.getElementById('n').innerHTML='<span class=w>Error: '+d.error+'</span>';return;}"
    "if(!d.aps||d.aps.length===0){document.getElementById('n').innerHTML='<span class=w>No networks</span>';return;}"
    "let h='';d.aps.forEach((x,i)=>{h+='<div class=e><div class=row>'+"
    "'<span class=n>'+x.ssid+'</span>'+"
    "'<span class=tag>CH'+x.ch+'</span>'+"
    "'<button onclick=as(\\''+x.ssid+'\\','+x.ch+') style=padding:2px 8px;font-size:11px>+</button></div></div>';});"
    "document.getElementById('n').innerHTML=h;}catch(e){document.getElementById('n').innerHTML='<span class=w>Error</span>';}}"
    "async function as(s,c){await fetch('/api/select',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s,channel:c})});ld();}"
    "async function ac(){let s=document.getElementById('c').value;if(s){await fetch('/api/select',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s,channel:6})});document.getElementById('c').value='';ld();}}"
    "async function tl(i){await fetch('/api/toggle-lock',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({i:i})});ld();}"
    "async function rm(i){await fetch('/api/select/delete?i='+i,{method:'DELETE'});ld();}"
    "async function ld(){try{let r=await fetch('/api/select');let d=await r.json();let h='';"
    "if(!d.ssids||d.ssids.length===0){document.getElementById('l').innerHTML='<div class=w>None</div>';return;}"
    "d.ssids.forEach((s,i)=>{h+='<div class=e><div class=row>'+"
    "'<button onclick=tl('+i+') style=padding:2px 6px;font-size:12px>'+(s.locked?'\\uD83D\\uDD12':'\\uD83D\\uDD13')+'</button>'+"
    "'<span class=n>'+s.ssid+'</span>'+"
    "'<span class=tag '+(s.locked?'tag-r':'tag-g')+'>'+(s.locked?'Locked':'Open')+'</span>'+"
    "'<button onclick=rm('+i+') style=padding:2px 8px;font-size:11px>&#x2716;</button></div></div>';});"
    "document.getElementById('l').innerHTML=h;}catch(e){}}"
    "async function sf(){await fetch('/api/spoof/start',{method:'POST'});up();}"
    "async function sp(){await fetch('/api/spoof/stop',{method:'POST'});up();}"
    "async function up(){try{let r=await fetch('/api/status');let d=await r.json();"
    "let el=document.getElementById('s');"
    "if(d.active){el.innerHTML='&#x25CF; Active | CH '+d.channel+' | '+d.count+' SSID';el.className='ok';}"
    "else{el.innerHTML='&#x25CF; Idle | '+d.count+' SSID';el.className='off';}}catch(e){}}"
    "async function lg(){try{let r=await fetch('/api/log');let d=await r.json();"
    "let h='<div class=lh><span style=width:130px>SSID</span><span style=width:44px>Try</span><span style=width:44px>Dev</span><span style=width:50px>RSSI</span><span style=width:150px>MAC</span><span style=width:60px>Vendor</span><span>Ago</span></div>';"
    "d.logs.forEach((x)=>{let a=x.seconds_ago<60?x.seconds_ago+'s':Math.floor(x.seconds_ago/60)+'m';"
    "let rc=x.rssi>-60?'tag-g':x.rssi>-75?'tag-o':'tag-r';"
    "h+='<div class=e><div class=row>'+"
    "'<span class=n>'+(x.locked?'\\uD83D\\uDD12 ':'\\uD83D\\uDD13 ')+x.ssid+'</span>'+"
    "'<span class=tag>'+x.attempts+'x</span>'+"
    "'<span class=tag>'+x.unique_devices+'d</span>'+"
    "'<span class=tag '+rc+'>'+x.rssi+'</span>'+"
    "'<span class=m>'+x.last_mac+'</span>'+"
    "'<span class=tag>'+x.vendor+'</span>'+"
    "'<span style=color:var(--muted);font-size:11px>'+a+'</span></div></div>';});"
    "document.getElementById('lg').innerHTML=h||'<div class=sc>No attempts</div>';}catch(e){}}"
    "async function rl(){await fetch('/api/log/reset',{method:'POST'});lg();}"
    "ld();lg();setInterval(up,2000);setInterval(lg,3000);"
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
            bool locked;
            if (beacon_get_ssid_at(i, ssid, &ch, &locked)) {
                if (i > 0) pos += sprintf(resp + pos, ",");
                pos += sprintf(resp + pos, "{\"ssid\":\"%s\",\"channel\":%d,\"locked\":%s}", ssid, ch, locked ? "true" : "false");
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

static esp_err_t api_log_handler(httpd_req_t *req)
{
    char *resp = malloc(4096);
    if (!resp) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    uint32_t now = xTaskGetTickCount() / 1000;
    int pos = 0;
    pos += sprintf(resp, "{\"logs\":[");
    int count = beacon_get_log_count();
    for (int i = 0, first = 1; i < count; i++) {
        log_entry_t e;
        if (beacon_get_log_at(i, &e)) {
            if (!first) pos += sprintf(resp + pos, ",");
            first = 0;
            uint32_t ago = (e.uptime_sec > 0 && now > e.uptime_sec) ? now - e.uptime_sec : 0;
            char ssid_esc[66] = {0};
            int j = 0;
            for (int k = 0; e.ssid[k]; k++) {
                if (e.ssid[k] == '"' || e.ssid[k] == '\\') ssid_esc[j++] = '\\';
                ssid_esc[j++] = e.ssid[k];
            }
            pos += sprintf(resp + pos, "{\"ssid\":\"%s\",\"locked\":%s,\"attempts\":%d,\"unique_devices\":%d,\"rssi\":%d,\"last_mac\":\"%s\",\"vendor\":\"%s\",\"seconds_ago\":%lu}",
                           ssid_esc, e.locked ? "true" : "false", e.attempts, e.unique_devices,
                           e.rssi, e.last_mac, e.vendor, (unsigned long)ago);
        }
    }
    pos += sprintf(resp + pos, "]}");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, pos);
    free(resp);
    return ESP_OK;
}

static esp_err_t api_log_reset_handler(httpd_req_t *req)
{
    beacon_reset_logs();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t api_toggle_lock_handler(httpd_req_t *req)
{
    char buf[64];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = 0;
        char *i_str = strstr(buf, "\"i\":");
        if (i_str) {
            int idx = atoi(i_str + 4);
            printf("API: toggle lock %d\n", idx);
            beacon_toggle_lock(idx);
        }
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
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
        {"/api/log", HTTP_GET, api_log_handler, NULL},
        {"/api/log/reset", HTTP_POST, api_log_reset_handler, NULL},
        {"/api/toggle-lock", HTTP_POST, api_toggle_lock_handler, NULL},
        {"/api/spoof/start", HTTP_POST, api_spoof_start_handler, NULL},
        {"/api/spoof/stop", HTTP_POST, api_spoof_stop_handler, NULL},
        {"/api/status", HTTP_GET, api_status_handler, NULL},
    };

    for (int i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(server, &uris[i]);
    }

    printf("HTTP: 192.168.4.1\n");
}
