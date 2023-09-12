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


#include <errno.h>
#include <stddef.h>
#include <string.h>


#include "led_strip.h"
#include "bluetooth_adv.h"

#define DELAY_TIME K_MSEC(1)




int	main(void)
{
	// int	err;
	int	counter;

	led_strip_init();
	bluetooth_init(); // Call the bluetooth_init() function
	bluetooth_mess();

	counter = 0;
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

		/* Sleep for 1 second */
		k_sleep(DELAY_TIME);
	}
	return (0);
}
