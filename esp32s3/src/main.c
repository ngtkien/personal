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

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include "led_strip.h"
#include "bluetooth_adv.h"

#define DELAY_TIME K_MSEC(100)



#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || \
	!DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified"
#endif

#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels,
			     DT_SPEC_AND_COMMA)
};

void adc_demo(){
	int err;
	uint32_t count = 0;
	uint16_t buf;
	struct adc_sequence sequence = {
		.buffer = &buf,
		/* buffer size in bytes, not number of samples */
		.buffer_size = sizeof(buf),
	};

	/* Configure channels individually prior to sampling. */
	for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
		if (!adc_is_ready_dt(&adc_channels[i])) {
			printk("ADC controller device %s not ready\n", adc_channels[i].dev->name);
			return 0;
		}

		err = adc_channel_setup_dt(&adc_channels[i]);
		if (err < 0) {
			printk("Could not setup channel #%d (%d)\n", i, err);
			return 0;
		}
	}

	/* Sleep for 1 second */
	printk("ADC reading[%u]:\n", count++);
	for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
		int32_t val_mv;

		printk("- %s, channel %d: ",
				adc_channels[i].dev->name,
				adc_channels[i].channel_id);

		(void)adc_sequence_init_dt(&adc_channels[i], &sequence);

		err = adc_read_dt(&adc_channels[i], &sequence);
		if (err < 0) {
			printk("Could not read (%d)\n", err);
			continue;
		}

		/*
			* If using differential mode, the 16 bit value
			* in the ADC sample buffer should be a signed 2's
			* complement value.
			*/
		if (adc_channels[i].channel_cfg.differential) {
			val_mv = (int32_t)((int16_t)buf);
		} else {
			val_mv = (int32_t)buf;
		}
		printk("%"PRId32, val_mv);
		err = adc_raw_to_millivolts_dt(&adc_channels[i],
							&val_mv);
		/* conversion to mV may not be supported, skip if not */
		if (err < 0) {
			printk(" (value in mV not available)\n");
		} else {
			printk(" = %"PRId32" mV\n", val_mv);
		}
	}
}

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
		if (counter % 500)
		{
			counter = 0;
			
			led_strip_demo();
			adc_demo();
		}



		
		/* Heartrate measurements simulation */
		hrs_notify();
		/* Battery level simulation */
		bas_notify();

		

		k_sleep(DELAY_TIME);
	}
	return (0);
}
