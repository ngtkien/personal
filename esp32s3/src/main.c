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

#define DELAY_TIME K_MSEC(1)

static const struct bt_data		ad[] = {BT_DATA_BYTES(BT_DATA_FLAGS,
			(BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
			BT_DATA_BYTES(BT_DATA_UUID16_ALL,
			BT_UUID_16_ENCODE(BT_UUID_HRS_VAL),
			BT_UUID_16_ENCODE(BT_UUID_BAS_VAL),
			BT_UUID_16_ENCODE(BT_UUID_DIS_VAL))};

static void	connected(struct bt_conn *conn, uint8_t err)
{
	if (err)
	{
		printk("Connection failed (err 0x%02x)\n", err);
	}
	else
	{
		printk("Connected\n");
	}
}

static void	disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason 0x%02x)\n", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void	bt_ready(void)
{
	int	err;

	printk("Bluetooth initialized\n");
	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err)
	{
		printk("Advertising failed to start (err %d)\n", err);
		return ;
	}
	printk("Advertising successfully started\n");
}

static void	auth_cancel(struct bt_conn *conn)
{
	char	addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb	auth_cb_display = {
	.cancel = auth_cancel,
};

static void	bas_notify(void)
{
	uint8_t	battery_level;

	battery_level = bt_bas_get_battery_level();
	battery_level--;
	if (!battery_level)
	{
		battery_level = 100U;
	}
	bt_bas_set_battery_level(battery_level);
}

static void	hrs_notify(void)
{
	static uint8_t	heartrate;

	heartrate = 90U;
	/* Heartrate measurements simulation */
	heartrate++;
	if (heartrate == 160U)
	{
		heartrate = 90U;
	}
	bt_hrs_notify(heartrate);
}

int	main(void)
{
	int	err;
	int	counter;

	led_strip_init();
	err = bt_enable(NULL);
	if (err)
	{
		printk("Bluetooth init failed (err %d)\n", err);
		return (0);
	}
	bt_ready();
	bt_conn_auth_cb_register(&auth_cb_display);
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
		k_sleep(DELAY_TIME);
	}
	return (0);
}
