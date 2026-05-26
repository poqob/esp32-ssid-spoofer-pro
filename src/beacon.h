#ifndef BEACON_H
#define BEACON_H

#include <stdbool.h>
#include <stdint.h>

void beacon_init(void);
void beacon_set_channel(uint8_t channel);
bool beacon_is_active(void);
void beacon_start(void);
void beacon_stop(void);
int beacon_get_count(void);
int beacon_get_ssid_count(void);
bool beacon_get_ssid_at(int index, char *out_ssid, uint8_t *out_channel, bool *out_locked);
int beacon_add_ssid(const char *ssid, uint8_t channel);
bool beacon_remove_ssid(int index);
uint8_t beacon_get_channel(void);
bool beacon_toggle_lock(int index);

#endif
