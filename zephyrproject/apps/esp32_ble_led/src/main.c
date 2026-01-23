#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>

/* CHANGE THIS IF NEEDED */
#define LED_PIN 2

static const struct device *gpio_dev;

/* Custom UUIDs */
#define BT_UUID_LED_SERVICE \
    BT_UUID_DECLARE_128(0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78, \
                        0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0)

#define BT_UUID_LED_CHAR \
    BT_UUID_DECLARE_128(0xab,0xcd,0xef,0x01,0xab,0xcd,0xef,0x01, \
                        0xab,0xcd,0xef,0x01,0x12,0x34,0x56,0x78)

/* Write callback */
static ssize_t write_led(struct bt_conn *conn,
                         const struct bt_gatt_attr *attr,
                         const void *buf, uint16_t len,
                         uint16_t offset, uint8_t flags)
{
    const uint8_t *data = buf;

    if (len < 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    if (data[0] == 0x31) {        /* ASCII '1' */
        gpio_pin_set(gpio_dev, LED_PIN, 1);
        printk("LED ON\n");
    } else if (data[0] == 0x30) { /* ASCII '0' */
        gpio_pin_set(gpio_dev, LED_PIN, 0);
        printk("LED OFF\n");
    }

    return len;
}

/* GATT Service */
BT_GATT_SERVICE_DEFINE(led_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_LED_SERVICE),
    BT_GATT_CHARACTERISTIC(
        BT_UUID_LED_CHAR,
        BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
        BT_GATT_PERM_WRITE,
        NULL,
        write_led,
        NULL
    ),
);

int main(void)
{
    int err;

    gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    if (!device_is_ready(gpio_dev)) {
        printk("GPIO not ready\n");
        return 0;
    }

    gpio_pin_configure(gpio_dev, LED_PIN, GPIO_OUTPUT_INACTIVE);

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    printk("Bluetooth initialized\n");

    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, NULL, 0, NULL, 0);
    if (err) {
        printk("Advertising failed (err %d)\n", err);
        return 0;
    }

    printk("Advertising started\n");

    while (1) {
        k_sleep(K_SECONDS(1));
    }
}
