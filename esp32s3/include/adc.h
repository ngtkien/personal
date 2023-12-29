#pragma once

#include <zephyr/drivers/adc.h>

int adc_init();
int esp_adc_read(int i);