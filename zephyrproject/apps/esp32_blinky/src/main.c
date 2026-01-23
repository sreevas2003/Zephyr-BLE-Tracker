#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define LED_GPIO_NODE DT_NODELABEL(gpio0)
#define LED_PIN 2   /* ESP32 DevKit V1 onboard LED (GPIO2) */

static const struct device *gpio = DEVICE_DT_GET(LED_GPIO_NODE);

int main(void)
{
    if (!device_is_ready(gpio)) {
        return 0;
    }

    gpio_pin_configure(gpio, LED_PIN, GPIO_OUTPUT_INACTIVE);

    while (1) {
        gpio_pin_toggle(gpio, LED_PIN);
        k_sleep(K_SECONDS(2));
    }
}

