#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(beacon, LOG_LEVEL_INF);

// 🔥 DIRECT GPIO (ESP32 LED = GPIO2)
#define LED_PIN 2

static const struct device *gpio_dev;

// ✅ FIXED BLE DATA (NO dynamic array)
static const uint8_t flags[] = {
    BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR
};

static const uint8_t manuf_data[] = {
    0xAF, 0xAF
};

void main(void)
{
    int err;

    LOG_INF("Beacon start");

    // 🔹 GPIO INIT
    gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

    if (!device_is_ready(gpio_dev)) {
        LOG_ERR("GPIO not ready");
        return;
    }

    gpio_pin_configure(gpio_dev, LED_PIN, GPIO_OUTPUT_ACTIVE);

    // 🔹 BLE INIT
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (%d)", err);
        return;
    }

    LOG_INF("Bluetooth initialized");

    // 🔹 Advertising
    struct bt_data ad[] = {
        BT_DATA(BT_DATA_FLAGS, flags, sizeof(flags)),
        BT_DATA(BT_DATA_MANUFACTURER_DATA, manuf_data, sizeof(manuf_data)),
    };

    err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Advertising failed (%d)", err);
        return;
    }

    LOG_INF("Advertising started");

    // 🔥 BLINK LOOP
    while (1) {
        gpio_pin_toggle(gpio_dev, LED_PIN);
        k_sleep(K_MSEC(500));
    }
}
