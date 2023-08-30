#pragma once

#define STRIP_NODE		DT_ALIAS(led_strip)
#define STRIP_NUM_PIXELS	DT_PROP(DT_ALIAS(led_strip), chain_length)
#define RGB(_r, _g, _b) { .r = (_r), .g = (_g), .b = (_b) }

int led_strip_init();
void led_strip_demo();