#include "led_strip.h"
#include <errno.h>
#include <string.h>

#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(led_strip);

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/types.h>

static const struct led_rgb	colors[] = {
	RGB(0x0f, 0x00, 0x00), /* red */
	RGB(0x00, 0x0f, 0x00), /* green */
	RGB(0x00, 0x00, 0x0f), /* blue */
};

struct led_rgb				pixels[STRIP_NUM_PIXELS];

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);

size_t						cursor = 0, color = 0;
int							rc;

int	led_strip_init(void)
{
	if (device_is_ready(strip))
	{
		LOG_INF("Found LED strip device %s", strip->name);
	}
	else
	{
		LOG_ERR("LED strip device %s is not ready", strip->name);
		return (0);
	}
	LOG_INF("Displaying pattern on strip");
	return (1);
}
void	led_strip_demo(void)
{
	memset(&pixels, 0x00, sizeof(pixels));
	memcpy(&pixels[cursor], &colors[color], sizeof(struct led_rgb));
	rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);

	if (rc)
	{
		LOG_ERR("couldn't update strip: %d", rc);
	}

	cursor++;
	if (cursor >= STRIP_NUM_PIXELS)
	{
		cursor = 0;
		color++;
		if (color == ARRAY_SIZE(colors))
		{
			color = 0;
		}
	}
}