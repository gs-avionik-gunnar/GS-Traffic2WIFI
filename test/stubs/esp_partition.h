#pragma once
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
typedef struct { uint32_t size; } esp_partition_t;
enum { ESP_PARTITION_TYPE_DATA = 1, ESP_PARTITION_SUBTYPE_DATA_SPIFFS = 0x82 };
const esp_partition_t* esp_partition_find_first(int type, int subtype, const char* label);
esp_err_t esp_partition_read(const esp_partition_t*, size_t off, void* dst, size_t n);
esp_err_t esp_partition_write(const esp_partition_t*, size_t off, const void* src, size_t n);
esp_err_t esp_partition_erase_range(const esp_partition_t*, size_t off, size_t n);
