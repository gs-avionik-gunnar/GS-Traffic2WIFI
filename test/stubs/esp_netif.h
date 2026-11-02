#pragma once
#include <stdint.h>
typedef struct esp_netif_obj esp_netif_t;
esp_netif_t* esp_netif_get_handle_from_ifkey(const char*);
int esp_netif_dhcps_stop(esp_netif_t*);
int esp_netif_dhcps_start(esp_netif_t*);
enum { ESP_NETIF_OP_SET = 1 };
enum { ESP_NETIF_ROUTER_SOLICITATION_ADDRESS = 3 };
int esp_netif_dhcps_option(esp_netif_t*, int, int, void*, uint32_t);
