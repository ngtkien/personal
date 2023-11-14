


#include "bluetooth_adv.h"
static int count = 0;
/* Custom Service Variables */
#define BT_UUID_CUSTOM_SERVICE_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)

static struct bt_uuid_128 primary_service_uuid = BT_UUID_INIT_128(
	BT_UUID_CUSTOM_SERVICE_VAL);

static struct bt_uuid_128 read_charac_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1));

static struct bt_uuid_128 write_charac_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2));

static struct bt_uuid_128 custom_charac_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef3));



/* Zeder Custom Service Variables */
#define BT_UUID_ZEDER_SERVICE_VAL \
	BT_UUID_128_ENCODE(0x02011995, 0x0201, 0x1995, 0x2808, 0xb92a4f188fd0)

static struct bt_uuid_128 zproject_service_uuid = BT_UUID_INIT_128(
	BT_UUID_ZEDER_SERVICE_VAL);


static struct bt_uuid_128 zproject_read_charac_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x02011995, 0x0201, 0x1995, 0x2808, 0xb92a4f188fd1));
static struct bt_uuid_128 zproject_write_charac_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x02011995, 0x0201, 0x1995, 0x2808, 0xb92a4f188fd2));
static struct bt_uuid_128 zproject_notify_charac_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x02011995, 0x0201, 0x1995, 0x2808, 0xb92a4f188fd3));
static struct bt_uuid_128 zproject_indicate_charac_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0x02011995, 0x0201, 0x1995, 0x2808, 0xb92a4f188fd4));

static int signed_value;
static struct bt_le_adv_param adv_param;
static int bond_count;

static ssize_t read_signed(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			   void *buf, uint16_t len, uint16_t offset)
{
	int *value = &signed_value;
	count++;
	*value = count;
	printk("Reading signed value\n,value: %d, offset: %d, len: %d\n",*value, offset, len);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 sizeof(signed_value));
}

static ssize_t write_signed(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			    const void *buf, uint16_t len, uint16_t offset,
			    uint8_t flags)
{
	int *value = &signed_value;

	if (offset + len > sizeof(signed_value)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	
	memcpy(value + offset, buf, len);
	printk("Writing signed value. Value: %d\n",*value);
	return len;	
}

/* Vendor Primary Service Declaration */
BT_GATT_SERVICE_DEFINE(primary_service,
	BT_GATT_PRIMARY_SERVICE(&primary_service_uuid),
	BT_GATT_CHARACTERISTIC(&read_charac_uuid.uuid,
			       BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ,
			       read_signed, NULL, NULL),
	BT_GATT_CHARACTERISTIC(&write_charac_uuid.uuid,
			       BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_WRITE_ENCRYPT,
			       NULL, write_signed, NULL),
	BT_GATT_CHARACTERISTIC(&custom_charac_uuid.uuid,
					BT_GATT_CHRC_READ,
					BT_GATT_PERM_READ,
					read_signed,NULL,NULL),
);

BT_GATT_SERVICE_DEFINE(zproject_service,
	BT_GATT_PRIMARY_SERVICE(&zproject_service_uuid),
	BT_GATT_CHARACTERISTIC(&zproject_read_charac_uuid.uuid,
			       BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ,
			       read_signed, NULL, NULL),
	BT_GATT_CHARACTERISTIC(&zproject_write_charac_uuid.uuid,
			       BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_WRITE_ENCRYPT,
			       NULL, write_signed, NULL),
	BT_GATT_CHARACTERISTIC(&zproject_notify_charac_uuid.uuid,
			       BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_WRITE_ENCRYPT,
			       NULL, write_signed, NULL),
	BT_GATT_CHARACTERISTIC(&zproject_indicate_charac_uuid.uuid,
			       BT_GATT_CHRC_INDICATE,
			       BT_GATT_PERM_WRITE_ENCRYPT,
			       NULL, write_signed, NULL),
);

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_CUSTOM_SERVICE_VAL)
};
static const struct bt_data		advertise_data[] = {BT_DATA_BYTES(BT_DATA_FLAGS,
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

static void add_bonded_addr_to_filter_list(const struct bt_bond_info *info, void *data)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_le_filter_accept_list_add(&info->addr);
	bt_addr_le_to_str(&info->addr, addr_str, sizeof(addr_str));
	printk("Added %s to advertising accept filter list\n", addr_str);
	bond_count++;
}

static void	bt_ready(void)
{
	int	err;

	printk("Bluetooth initialized\n");
	//Add
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	bond_count = 0;
	
	bt_foreach_bond(BT_ID_DEFAULT, add_bonded_addr_to_filter_list, NULL);

	adv_param = *BT_LE_ADV_CONN_NAME;


	printk("Bond count: %d\n", bond_count);
	/* If we have got at least one bond, activate the filter */
	if (bond_count) {
		/* BT_LE_ADV_OPT_FILTER_CONN is required to activate accept filter list,
		 * BT_LE_ADV_OPT_FILTER_SCAN_REQ will prevent sending scan response data to
		 * devices, that are not on the accept filter list
		 */
		adv_param.options |= BT_LE_ADV_OPT_FILTER_CONN | BT_LE_ADV_OPT_FILTER_SCAN_REQ;
	}

	// Add the advertise data to the advertisement
	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, advertise_data, ARRAY_SIZE(advertise_data), NULL, 0);
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


void pairing_complete(struct bt_conn *conn, bool bonded)
{
	printk("Pairing completed. Rebooting in 5 seconds...\n");

	k_sleep(K_SECONDS(5));
	sys_reboot(SYS_REBOOT_WARM);
}

static struct bt_conn_auth_info_cb bt_conn_auth_info = {
	.pairing_complete = pairing_complete
};
void bas_notify()
{
	uint8_t	battery_level;

	battery_level = bt_bas_get_battery_level();
	battery_level--;
	if (!battery_level)
	{
		battery_level = 100U;
	}
	// printk("Battery level: %d\n", battery_level);
	bt_bas_set_battery_level(battery_level);
}

void hrs_notify()
{
	static uint8_t	heartrate;

	heartrate = 90U;
	/* Heartrate measurements simulation */
	heartrate++;
	if (heartrate == 160U)
	{
		heartrate = 90U;
	}
	// printk("Heartrate: %d\n", heartrate);
	bt_hrs_notify(heartrate);
}

void bluetooth_init()
{
    int err;
    // Bluetooth initialization code
    err = bt_enable(NULL);
	if (err)
	{
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}
	bt_ready();
	bt_conn_auth_cb_register(&auth_cb_display);
	bt_conn_auth_info_cb_register(&bt_conn_auth_info);
}
void bluetooth_mess(){
	printk("Bluetooth ok\n");
}