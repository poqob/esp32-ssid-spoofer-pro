#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"
#include "config.h"
#include "beacon.h"

typedef struct { uint8_t oui[3]; const char *name; } oui_entry_t;

static const oui_entry_t OUI_LIST[] = {
    {{0x00,0x03,0x93},"Apple"},{{0x00,0x17,0xF2},"Apple"},{{0x00,0x1B,0x63},"Apple"},
    {{0x00,0x1E,0xC2},"Apple"},{{0x00,0x1F,0x5B},"Apple"},{{0x00,0x21,0xE9},"Apple"},
    {{0x00,0x22,0x41},"Apple"},{{0x00,0x23,0x32},"Apple"},{{0x00,0x23,0x6C},"Apple"},
    {{0x00,0x24,0x9E},"Apple"},{{0x04,0x0C,0xCE},"Apple"},{{0x04,0x15,0x52},"Apple"},
    {{0x04,0x26,0x65},"Apple"},{{0x04,0x2E,0x01},"Apple"},{{0x04,0x61,0x59},"Apple"},
    {{0x08,0x66,0x98},"Apple"},{{0x0C,0x30,0x21},"Apple"},{{0x0C,0x74,0xC2},"Apple"},
    {{0x10,0x40,0xF3},"Apple"},{{0x14,0x7D,0xDA},"Apple"},{{0x18,0x65,0x90},"Apple"},
    {{0x18,0xEE,0x69},"Apple"},{{0x1C,0x91,0x80},"Apple"},{{0x20,0x2B,0x20},"Apple"},
    {{0x24,0x1E,0xEB},"Apple"},{{0x24,0xA0,0x74},"Apple"},{{0x28,0x37,0x37},"Apple"},
    {{0x28,0xCF,0xDA},"Apple"},{{0x2C,0xBE,0x08},"Apple"},{{0x30,0x10,0xE4},"Apple"},
    {{0x34,0x59,0x9B},"Apple"},{{0x34,0xC0,0x59},"Apple"},{{0x38,0x48,0x4C},"Apple"},
    {{0x38,0x87,0xD5},"Apple"},{{0x3C,0x07,0x54},"Apple"},{{0x3C,0x22,0xFB},"Apple"},
    {{0x40,0x6C,0x8F},"Apple"},{{0x40,0xA8,0xF0},"Apple"},{{0x44,0x00,0x10},"Apple"},
    {{0x48,0x43,0x7C},"Apple"},{{0x48,0x60,0xBC},"Apple"},{{0x4C,0x32,0x75},"Apple"},
    {{0x50,0x3D,0xE5},"Apple"},{{0x54,0x26,0x96},"Apple"},{{0x58,0x1F,0x28},"Apple"},
    {{0x5C,0x0A,0x5B},"Apple"},{{0x5C,0xE0,0xC5},"Apple"},{{0x60,0x30,0xD4},"Apple"},
    {{0x60,0x92,0x17},"Apple"},{{0x64,0x20,0x0C},"Apple"},{{0x64,0x9E,0x31},"Apple"},
    {{0x6C,0x72,0xE7},"Apple"},{{0x70,0x14,0xA6},"Apple"},{{0x74,0xE1,0xB6},"Apple"},
    {{0x78,0x4F,0x43},"Apple"},{{0x80,0xBE,0x05},"Apple"},{{0x84,0x38,0x35},"Apple"},
    {{0x88,0x66,0x5A},"Apple"},{{0x8C,0x85,0x90},"Apple"},{{0x90,0x84,0x0D},"Apple"},
    {{0x94,0xBF,0x2E},"Apple"},{{0x98,0x01,0xA7},"Apple"},{{0x9C,0x20,0x7B},"Apple"},
    {{0xA0,0x78,0x17},"Apple"},{{0xA4,0xD1,0xD2},"Apple"},{{0xA8,0x51,0xAB},"Apple"},
    {{0xAC,0x29,0x3A},"Apple"},{{0xB0,0x34,0x95},"Apple"},{{0xB4,0x8C,0x9D},"Apple"},
    {{0xB8,0x53,0xAC},"Apple"},{{0xBC,0x4C,0xC4},"Apple"},{{0xC0,0x8B,0x6F},"Apple"},
    {{0xC4,0x2B,0x2C},"Apple"},{{0xC8,0xB5,0xB7},"Apple"},{{0xCC,0x08,0xE0},"Apple"},
    {{0xD0,0x03,0x4B},"Apple"},{{0xD4,0x61,0x9D},"Apple"},{{0xD8,0x1C,0x79},"Apple"},
    {{0xDC,0x2C,0x26},"Apple"},{{0xE0,0xCE,0xC3},"Apple"},{{0xE4,0xE0,0x2E},"Apple"},
    {{0xE8,0xC7,0x9F},"Apple"},{{0xEC,0x85,0x2F},"Apple"},{{0xF0,0x18,0x98},"Apple"},
    {{0xF0,0xC1,0xF1},"Apple"},{{0xF4,0x0F,0x24},"Apple"},{{0xF4,0x5C,0x89},"Apple"},
    {{0xF8,0x0D,0xA9},"Apple"},{{0xFC,0xE9,0x98},"Apple"},
    {{0x00,0x23,0xD4},"Samsung"},{{0x5C,0x49,0x79},"Samsung"},{{0x00,0x15,0x99},"Samsung"},
    {{0x00,0x1E,0x6B},"Samsung"},{{0x38,0xBC,0x1A},"Samsung"},{{0x4C,0x5E,0x0C},"Samsung"},
    {{0x58,0xA2,0x39},"Samsung"},{{0x64,0x1C,0x67},"Samsung"},{{0x70,0x48,0x0F},"Samsung"},
    {{0x74,0x2B,0x0F},"Samsung"},{{0x78,0x60,0xA0},"Samsung"},{{0x7C,0xBB,0x8A},"Samsung"},
    {{0x84,0xDB,0x2F},"Samsung"},{{0x88,0x74,0xE6},"Samsung"},{{0x90,0x17,0xC8},"Samsung"},
    {{0x94,0x19,0xD8},"Samsung"},{{0x98,0x68,0xF0},"Samsung"},{{0xA4,0x90,0xAD},"Samsung"},
    {{0xAC,0x5F,0x3E},"Samsung"},{{0xB0,0xD7,0x6F},"Samsung"},{{0xDC,0x0B,0x34},"Samsung"},
    {{0xE0,0x63,0xDA},"Samsung"},{{0xE4,0xB9,0x7A},"Samsung"},{{0xEC,0x1A,0x59},"Samsung"},
    {{0x78,0x23,0xAE},"Xiaomi"},{{0xFC,0xA1,0x3F},"Xiaomi"},{{0x9C,0xD2,0x1F},"Xiaomi"},
    {{0x48,0xE7,0x29},"Xiaomi"},{{0xAC,0x37,0x43},"Xiaomi"},{{0x10,0x18,0x05},"Xiaomi"},
    {{0x68,0xDD,0x26},"Xiaomi"},{{0xA0,0xE9,0xDB},"Xiaomi"},{{0x04,0xCF,0x8C},"Xiaomi"},
    {{0x48,0xE7,0x29},"Huawei"},{{0x84,0x25,0xDB},"Huawei"},{{0x18,0x8B,0x9D},"Huawei"},
    {{0x2C,0x59,0x8A},"Huawei"},{{0x34,0xCC,0x93},"Huawei"},{{0x44,0x6C,0xC0},"Huawei"},
    {{0x48,0x54,0x6A},"Huawei"},{{0x60,0x5A,0x44},"Huawei"},{{0x7C,0xED,0x8D},"Google"},
    {{0xA4,0x77,0x33},"Google"},{{0x18,0x1D,0xEA},"Google"},{{0x24,0x18,0x1D},"Google"},
    {{0x44,0xAF,0x28},"Google"},{{0x50,0x4D,0x20},"Google"},{{0x8C,0xB8,0x4D},"Google"},
    {{0x9C,0xB7,0x0D},"Google"},{{0xA0,0xC9,0xA0},"Google"},{{0xBC,0x9C,0x31},"Google"},
    {{0x00,0x26,0xF2},"Intel"},{{0x00,0x1A,0x11},"Intel"},{{0x00,0x0C,0xE7},"Intel"},
    {{0x34,0x95,0xDB},"Intel"},{{0x50,0x3E,0xAA},"Intel"},{{0x7C,0xDD,0x90},"Intel"},
    {{0x00,0x1E,0x64},"Qualcomm"},{{0x00,0x1A,0xA1},"Qualcomm"},{{0x40,0x70,0x22},"Qualcomm"},
    {{0x74,0xDA,0x38},"Qualcomm"},{{0x48,0xAD,0x08},"OnePlus"},{{0x9A,0x05,0x51},"OnePlus"},
    {{0xBA,0x2B,0x87},"OnePlus"},{{0xFA,0x6D,0xB4},"OnePlus"},{{0x58,0x98,0x7A},"Oppo"},
    {{0x6C,0x02,0xE0},"Oppo"},{{0x70,0x39,0x67},"Oppo"},{{0x00,0x21,0x5C},"Nokia"},
    {{0x00,0x1E,0x3A},"Nokia"},{{0x28,0xE0,0x2C},"Nokia"},{{0x48,0x2C,0xA0},"Nokia"},
    {{0x00,0x1B,0xA5},"LG"},{{0x04,0xCB,0x4E},"LG"},{{0x80,0x1D,0x60},"LG"},
    {{0x00,0x24,0xBA},"Sony"},{{0x44,0xD4,0x53},"Sony"},{{0x00,0x21,0x5A},"Sony"},
    {{0x00,0x22,0x5F},"Motorola"},{{0x14,0xCF,0x92},"Motorola"},{{0x2C,0xBE,0x97},"Motorola"},
    {{0x00,0x26,0xE1},"Lenovo"},{{0xEC,0x8C,0xA1},"Lenovo"},{{0x00,0x1B,0xD5},"Asus"},
    {{0x60,0xA4,0x4C},"Asus"},{{0x10,0x7B,0x44},"Asus"},{{0x90,0x9A,0x4A},"RPi"},
    {{0x38,0x07,0x1E},"RPi"},{{0x3C,0x24,0xF2},"RPi"},{{0xC8,0x2A,0x14},"RPi"},
    {{0xDC,0xA6,0x32},"RPi"},{{0xD8,0x3A,0xDD},"RPi"},{{0xE4,0x5F,0x01},"RPi"},
    {{0xAC,0xD1,0xB8},"Espressif"},{{0x24,0x0A,0xC4},"Espressif"},{{0x24,0x62,0xAB},"Espressif"},
    {{0x68,0xC6,0x3A},"Espressif"},{{0x84,0x0D,0x8E},"Espressif"},
    {{0x84,0xF7,0x03},"Espressif"},{{0x8C,0xCE,0x4E},"Espressif"},{{0x10,0x52,0x1C},"Realtek"},
    {{0x00,0xE0,0x4C},"Realtek"},{{0x20,0x47,0xDA},"Realtek"},{{0x00,0x26,0x06},"Broadcom"},
    {{0x00,0x10,0x18},"Broadcom"},{{0x00,0x25,0x9E},"MediaTek"},{{0x00,0x1C,0x4A},"MediaTek"},
    {{0x10,0x7B,0xEF},"Amazon"},{{0x74,0x75,0x48},"Amazon"},{{0xE0,0xBF,0x31},"Amazon"},
    {{0x00,0x0E,0x8F},"TP-Link"},{{0x14,0xCF,0xE5},"TP-Link"},{{0x50,0xC7,0xBF},"TP-Link"},
    {{0x20,0xAA,0x4B},"TP-Link"},{{0x24,0x69,0x68},"TP-Link"},{{0x84,0x0F,0x6E},"TP-Link"},
    {{0x30,0xD7,0x89},"Netgear"},{{0x5C,0x63,0xBF},"Netgear"},{{0x9C,0x3D,0xCF},"Netgear"},
};

static const char* lookup_vendor(const uint8_t *mac)
{
    for (int i = 0; i < sizeof(OUI_LIST)/sizeof(OUI_LIST[0]); i++) {
        if (mac[0] == OUI_LIST[i].oui[0] &&
            mac[1] == OUI_LIST[i].oui[1] &&
            mac[2] == OUI_LIST[i].oui[2])
            return OUI_LIST[i].name;
    }
    return "Bilinmiyor";
}

static char selected_ssids[MAX_SSID][33];
static bool locked_ssids[MAX_SSID];
static int attempts_ssids[MAX_SSID];
static int unique_counts[MAX_SSID];
static int8_t rssi_ssids[MAX_SSID];
static char last_mac_strs[MAX_SSID][18];
static uint8_t mac_lists[MAX_SSID][16][6];
static uint32_t last_seen_ssids[MAX_SSID];
static int selected_count = 0;
static bool spoof_active = false;
static int spoof_channel = AP_CHANNEL;
static SemaphoreHandle_t mutex = NULL;
static TaskHandle_t spoof_task_handle = NULL;

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

    if (xSemaphoreTake(mutex, 0) != pdTRUE) return;

    for (int j = 0; j < selected_count; j++) {
        if (strcmp(selected_ssids[j], ssid) == 0) {
            attempts_ssids[j]++;
            last_seen_ssids[j] = xTaskGetTickCount() / 1000;
            rssi_ssids[j] = pkt->rx_ctrl.rssi;

            uint8_t *mac = &frame[10];
            int found = 0;
            for (int m = 0; m < unique_counts[j]; m++) {
                if (memcmp(mac_lists[j][m], mac, 6) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found && unique_counts[j] < 16) {
                memcpy(mac_lists[j][unique_counts[j]], mac, 6);
                unique_counts[j]++;
            }

            snprintf(last_mac_strs[j], sizeof(last_mac_strs[j]),
                     "%02x:%02x:%02x:%02x:%02x:%02x",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

            printf("DENEME: %s <- %s (%d,%dc) RSSI:%d %s\n", ssid,
                   locked_ssids[j] ? "0174658631" : "(sifresiz)",
                   attempts_ssids[j], unique_counts[j],
                   rssi_ssids[j], last_mac_strs[j]);
            break;
        }
    }
    xSemaphoreGive(mutex);
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
    attempts_ssids[selected_count] = 0;
    unique_counts[selected_count] = 0;
    rssi_ssids[selected_count] = 0;
    last_mac_strs[selected_count][0] = 0;
    last_seen_ssids[selected_count] = 0;
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
        attempts_ssids[j] = attempts_ssids[j+1];
        unique_counts[j] = unique_counts[j+1];
        rssi_ssids[j] = rssi_ssids[j+1];
        strcpy(last_mac_strs[j], last_mac_strs[j+1]);
        memcpy(mac_lists[j], mac_lists[j+1], sizeof(mac_lists[j]));
        last_seen_ssids[j] = last_seen_ssids[j+1];
    }
    selected_ssids[selected_count-1][0] = 0;
    locked_ssids[selected_count-1] = false;
    attempts_ssids[selected_count-1] = 0;
    unique_counts[selected_count-1] = 0;
    rssi_ssids[selected_count-1] = 0;
    last_mac_strs[selected_count-1][0] = 0;
    last_seen_ssids[selected_count-1] = 0;
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

void beacon_reset_logs(void)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
    for (int i = 0; i < selected_count; i++) {
        attempts_ssids[i] = 0;
        unique_counts[i] = 0;
        rssi_ssids[i] = 0;
        last_mac_strs[i][0] = 0;
        last_seen_ssids[i] = 0;
    }
    xSemaphoreGive(mutex);
}

int beacon_get_log_count(void)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
    int count = 0;
    for (int i = 0; i < selected_count; i++) {
        if (attempts_ssids[i] > 0) count++;
    }
    xSemaphoreGive(mutex);
    return count;
}

bool beacon_get_log_at(int index, log_entry_t *out)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
    int nth = 0;
    for (int i = 0; i < selected_count; i++) {
        if (attempts_ssids[i] > 0) {
            if (nth == index) {
                strcpy(out->ssid, selected_ssids[i]);
                out->locked = locked_ssids[i];
                out->attempts = attempts_ssids[i];
                out->unique_devices = unique_counts[i];
                out->rssi = rssi_ssids[i];
                strcpy(out->last_mac, last_mac_strs[i]);
                if (unique_counts[i] > 0) {
                    strncpy(out->vendor, lookup_vendor(mac_lists[i][0]), sizeof(out->vendor) - 1);
                } else {
                    strcpy(out->vendor, "?");
                }
                out->vendor[sizeof(out->vendor) - 1] = 0;
                out->uptime_sec = last_seen_ssids[i];
                xSemaphoreGive(mutex);
                return true;
            }
            nth++;
        }
    }
    xSemaphoreGive(mutex);
    return false;
}
