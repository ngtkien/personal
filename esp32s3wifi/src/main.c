/*
 * Copyright (c) 2017 Linaro Limited
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);


#include <stddef.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/types.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/reboot.h>


#include <zephyr/net/wifi.h>

#include "led_strip.h"


#define DELAY_TIME K_MSEC(1)




int	main(void)
{
	// int	err;
	int	counter;

	led_strip_init();

	counter = 0;

	printk("Starting...\n");
	/* Implement notification. At the moment there is no suitable way
		* of starting delayed work so we do it here
		*/
	while (1)
	{
		counter++;
		if (counter == 50)
		{
			counter = 0;
			
			led_strip_demo();
		}


		

		k_sleep(DELAY_TIME);
	}
	return (0);
}
