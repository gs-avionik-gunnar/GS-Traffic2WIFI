#pragma once
#include <stdint.h>
uint32_t REG_READ(uint32_t);
void REG_WRITE(uint32_t, uint32_t);
#define GPIO_IN1_REG 1
#define GPIO_OUT_W1TS_REG 2
#define GPIO_OUT_W1TC_REG 3
