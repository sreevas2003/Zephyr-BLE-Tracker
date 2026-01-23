#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/sys/printk.h>

/* Fixed UUID for single-beacon filtering */
static const uint8_t beacon_uuid[16] = {
    0x12, 0x34, 0x56, 0x78,
    0x90, 0xAB,
    0xCD, 0xEF,
    0x12, 0x34,
    0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF
};

static const struct bt_data ad[] = {
    BT_DATA(BT_DATA_UUID128_ALL, beacon_uuid, 16),
};

int main(void)
{
    int err;

    printk("BLE Beacon starting...\n");

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    err = bt_le_adv_start(BT_LE_ADV_NCONN,
                          ad, ARRAY_SIZE(ad),
                          NULL, 0);
    if (err) {
        printk("Advertising failed (err %d)\n", err);
        return 0;
    }

    printk("Beacon advertising started\n");

    while (1) {
        k_sleep(K_SECONDS(5));
        printk("Beacon alive\n");
    }
}
