#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"
#include "config.h"
#include "beacon.h"

static char selected_ssids[MAX_SSID][33];
static bool locked_ssids[MAX_SSID];
static int selected_count = 0;
static bool spoof_active = false;
static int spoof_channel = AP_CHANNEL;
static SemaphoreHandle_t mutex = NULL;
static TaskHandle_t spoof_task_handle = NULL;

#define MAX_LOG 30
static log_entry_t log_buffer[MAX_LOG];
static int log_write_idx = 0;
static int log_count = 0;
static SemaphoreHandle_t log_mutex = NULL;

static void add_log(const char *mac, const char *ssid, bool locked)
{
    xSemaphoreTake(log_mutex, portMAX_DELAY);
    int idx = log_write_idx;
    strncpy(log_buffer[idx].mac, mac, sizeof(log_buffer[idx].mac) - 1);
    log_buffer[idx].mac[sizeof(log_buffer[idx].mac) - 1] = 0;
    strncpy(log_buffer[idx].ssid, ssid, sizeof(log_buffer[idx].ssid) - 1);
    log_buffer[idx].ssid[sizeof(log_buffer[idx].ssid) - 1] = 0;
    log_buffer[idx].locked = locked;
    log_write_idx = (log_write_idx + 1) % MAX_LOG;
    if (log_count < MAX_LOG) log_count++;
    xSemaphoreGive(log_mutex);
    printf("LOG: %s -> %s (%s)\n", mac, ssid, locked ? "LOCKED" : "OPEN");
}

static void promiscuous_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t *frame = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;

    if (len < 24) return;
    if ((frame[0] & 0x0C) != 0x00) return;

    uint8_t subtype = frame[0] >> 4;
    bool is_assoc = (subtype == 0x00);
    bool is_probe = (subtype == 0x04);
    if (!is_assoc && !is_probe) return;

    char mac[18];
    snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
             frame[10], frame[11], frame[12], frame[13], frame[14], frame[15]);

    int body = 24;
    if (is_assoc) body += 4;

    char ssid[33] = "";
    int i = body;
    while (i + 2 <= len) {
        uint8_t tag = frame[i];
        uint8_t tlen = frame[i + 1];
        if (tag == 0x00 && tlen > 0 && tlen <= 32 && i + 2 + tlen <= len) {
            memcpy(ssid, &frame[i + 2], tlen);
            ssid[tlen] = 0;
            break;
        }
        i += 2 + tlen;
    }

    if (strlen(ssid) == 0) return;

    bool locked = false;
    if (xSemaphoreTake(mutex, 0) == pdTRUE) {
        for (int j = 0; j < selected_count; j++) {
            if (strcmp(selected_ssids[j], ssid) == 0) {
                locked = locked_ssids[j];
                break;
            }
        }
        xSemaphoreGive(mutex);
    }
    add_log(mac, ssid, locked);
}

static int build_beacon_frame(uint8_t *frame, const char *ssid, uint8_t idx, uint8_t channel, bool locked)
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

    if (locked) {
        static const uint8_t rsn_ie[] = {
            0x30, 0x14, 0x01, 0x00, 0x00, 0x0F, 0xAC, 0x04,
            0x01, 0x00, 0x00, 0x0F, 0xAC, 0x04, 0x01, 0x00,
            0x00, 0x0F, 0xAC, 0x02, 0x00, 0x00
        };
        memcpy(&frame[pos], rsn_ie, sizeof(rsn_ie));
        pos += sizeof(rsn_ie);
    }

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
        bool locked[MAX_SSID];
        int count = selected_count;
        for (int i = 0; i < count; i++) {
            strcpy(ssids[i], selected_ssids[i]);
            locked[i] = locked_ssids[i];
        }
        xSemaphoreGive(mutex);

        if (!active || count == 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);

        for (int s = 0; s < count; s++) {
            for (int b = 0; b < BEACON_BURST; b++) {
                int len = build_beacon_frame(frame, ssids[s], s, ch, locked[s]);
                esp_wifi_80211_tx(WIFI_IF_AP, frame, len, false);
            }
            vTaskDelay(pdMS_TO_TICKS(BEACON_INTERVAL_MS));
        }
    }
    free(frame);
}

void beacon_init(void)
{
    mutex = xSemaphoreCreateMutex();
    log_mutex = xSemaphoreCreateMutex();

    esp_wifi_set_promiscuous_rx_cb(promiscuous_cb);
    esp_wifi_set_promiscuous(true);

    xTaskCreate(spoof_task, "spoof", 4096, NULL, 5, &spoof_task_handle);
}

void beacon_set_channel(uint8_t channel)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
    spoof_channel = channel;
    xSemaphoreGive(mutex);
}

bool beacon_is_active(void)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool active = spoof_active;
    xSemaphoreGive(mutex);
    return active;
}

void beacon_start(void)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
    spoof_active = true;
    xSemaphoreGive(mutex);
}

void beacon_stop(void)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
    spoof_active = false;
    esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE);
    xSemaphoreGive(mutex);
}

int beacon_get_count(void)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
    int count = selected_count;
    xSemaphoreGive(mutex);
    return count;
}

int beacon_add_ssid(const char *ssid, uint8_t channel)
{
    int slen = strlen(ssid);
    if (slen == 0 || slen > 32) return -1;

    xSemaphoreTake(mutex, portMAX_DELAY);
    if (selected_count >= MAX_SSID) {
        xSemaphoreGive(mutex);
        return -1;
    }
    strncpy(selected_ssids[selected_count], ssid, slen);
    selected_ssids[selected_count][slen] = 0;
    locked_ssids[selected_count] = false;
    spoof_channel = channel;
    selected_count++;
    int idx = selected_count - 1;
    xSemaphoreGive(mutex);

    printf("Added SSID: %s CH:%d\n", selected_ssids[idx], spoof_channel);
    return idx;
}

int beacon_get_ssid_count(void)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
    int count = selected_count;
    xSemaphoreGive(mutex);
    return count;
}

bool beacon_get_ssid_at(int index, char *out_ssid, uint8_t *out_channel, bool *out_locked)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (index < 0 || index >= selected_count) {
        xSemaphoreGive(mutex);
        return false;
    }
    strcpy(out_ssid, selected_ssids[index]);
    *out_channel = spoof_channel;
    *out_locked = locked_ssids[index];
    xSemaphoreGive(mutex);
    return true;
}

bool beacon_remove_ssid(int index)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (index < 0 || index >= selected_count) {
        xSemaphoreGive(mutex);
        return false;
    }
    for (int j = index; j < selected_count - 1; j++) {
        strcpy(selected_ssids[j], selected_ssids[j+1]);
        locked_ssids[j] = locked_ssids[j+1];
    }
    selected_ssids[selected_count-1][0] = 0;
    locked_ssids[selected_count-1] = false;
    selected_count--;
    printf("Removed SSID at %d, now %d SSIDs\n", index, selected_count);
    xSemaphoreGive(mutex);
    return true;
}

uint8_t beacon_get_channel(void)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
    uint8_t ch = spoof_channel;
    xSemaphoreGive(mutex);
    return ch;
}

bool beacon_toggle_lock(int index)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (index < 0 || index >= selected_count) {
        xSemaphoreGive(mutex);
        return false;
    }
    locked_ssids[index] = !locked_ssids[index];
    printf("Toggled lock SSID[%d] %s -> %s\n", index, selected_ssids[index],
           locked_ssids[index] ? "LOCKED" : "OPEN");
    xSemaphoreGive(mutex);
    return true;
}

int beacon_get_log_count(void)
{
    xSemaphoreTake(log_mutex, portMAX_DELAY);
    int count = log_count;
    xSemaphoreGive(log_mutex);
    return count;
}

bool beacon_get_log_at(int index, log_entry_t *out)
{
    xSemaphoreTake(log_mutex, portMAX_DELAY);
    if (index < 0 || index >= log_count) {
        xSemaphoreGive(log_mutex);
        return false;
    }
    int start;
    if (log_count < MAX_LOG) {
        start = 0;
    } else {
        start = log_write_idx;
    }
    int idx = (start + index) % MAX_LOG;
    memcpy(out, &log_buffer[idx], sizeof(log_entry_t));
    xSemaphoreGive(log_mutex);
    return true;
}
