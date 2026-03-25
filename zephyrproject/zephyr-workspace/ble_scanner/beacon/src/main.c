#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <stdio.h>

/*
 * 🔥 Manufacturer Data Format
 * 0xAF 0xAF → YOUR PROJECT SIGNATURE
 * 0x01      → Version / type (optional)
 * 0x00      → Reserved
 */
static const struct bt_data ad[] = {
    /* Flags */
    BT_DATA_BYTES(BT_DATA_FLAGS,
                  (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),

    /* 🔥 Manufacturer specific data */
    BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA,
                  0xAF, 0xAF,   // Company ID (your signature)
                  0x01,         // version
                  0x00)         // reserved
};

int main(void)
{
    int err;

    printf("Beacon starting...\n");

    err = bt_enable(NULL);
    if (err) {
        printf("Bluetooth init failed (%d)\n", err);
        return 0;
    }

    /* Start non-connectable advertising */
    err = bt_le_adv_start(BT_LE_ADV_NCONN,
                          ad, ARRAY_SIZE(ad),
                          NULL, 0);

    if (err) {
        printf("Advertising failed (%d)\n", err);
        return 0;
    }

    printf("Beacon advertising...\n");

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
