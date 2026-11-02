#pragma once
#include <stdint.h>
typedef enum { WIFI_COUNTRY_POLICY_MANUAL = 1 } wifi_country_policy_t;
typedef struct { char cc[3]; uint8_t schan; uint8_t nchan; int8_t max_tx_power; wifi_country_policy_t policy; } wifi_country_t;
typedef int esp_err_t;
#define ESP_OK 0
esp_err_t esp_wifi_set_country(const wifi_country_t*);
