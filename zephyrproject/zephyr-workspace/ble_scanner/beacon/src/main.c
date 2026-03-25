#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(beacon, LOG_LEVEL_INF);

// 🔥 LED CONFIG (ESP32 DevKit default = GPIO2)
#define LED_NODE DT_ALIAS(led0)

#if !DT_NODE_HAS_STATUS(LED_NODE, okay)
#error "LED not defined in devicetree"
#endif

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

void main(void)
{
    int err;

    LOG_INF("Beacon starting...");

    // 🔹 Init LED
    if (!gpio_is_ready_dt(&led)) {
        LOG_ERR("LED device not ready");
        return;
    }

    gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

    // 🔹 Init BLE
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (%d)", err);
        return;
    }

    LOG_INF("Bluetooth initialized");

    // 🔹 Advertising data
    static const struct bt_data ad[] = {
        BT_DATA(BT_DATA_FLAGS, (uint8_t[]){BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR}, 1),
        BT_DATA(BT_DATA_MANUFACTURER_DATA, (uint8_t[]){0xAF, 0xAF}, 2),
    };

    err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Advertising failed (%d)", err);
        return;
    }

    LOG_INF("Beacon advertising started");

    // 🔥 LED BLINK LOOP
    while (1) {
        gpio_pin_toggle_dt(&led);
        k_sleep(K_MSEC(500));   // 500ms blink
    }
}
