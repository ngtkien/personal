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

#include "led_strip.h"
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/services/hrs.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/types.h>


#include "bluetooth_adv.h"

#define DELAY_TIME K_MSEC(1)





int	main(void)
{
	// int	err;
	int	counter;

	led_strip_init();
	bluetooth_init(); // Call the bluetooth_init() function

	// counter = 0;
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
		/* Heartrate measurements simulation */
		hrs_notify();
		/* Battery level simulation */
		bas_notify();
		k_sleep(DELAY_TIME);
	}
	return (0);
}
