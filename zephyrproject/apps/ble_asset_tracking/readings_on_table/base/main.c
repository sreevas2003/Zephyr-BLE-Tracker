#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/mqtt.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

LOG_MODULE_REGISTER(ble_base, LOG_LEVEL_INF);

#define MQTT_TOPIC "asset/table"
#define MQTT_PORT  1883

static struct mqtt_client client;
static struct sockaddr_in broker;
static struct net_mgmt_event_callback net_cb;

static uint16_t msg_id;
K_SEM_DEFINE(wifi_ready, 0, 1);

/* Wi-Fi event */
static void net_event_handler(struct net_mgmt_event_callback *cb,
                              uint32_t mgmt_event,
                              struct net_if *iface)
{
    if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
        LOG_INF("Wi-Fi connected, IP ready");
        k_sem_give(&wifi_ready);
    }
}

/* MQTT init */
static void mqtt_init(void)
{
    mqtt_client_init(&client);

    broker.sin_family = AF_INET;
    broker.sin_port = htons(MQTT_PORT);
    broker.sin_addr.s_addr = htonl(0x7F000001); /* 127.0.0.1 */

    client.broker = &broker;
    client.client_id.utf8 = "esp32_base";
    client.client_id.size = strlen(client.client_id.utf8);
    client.protocol_version = MQTT_VERSION_3_1_1;
}

/* BLE scan callback */
static void device_found(const bt_addr_le_t *addr, int8_t rssi,
                         uint8_t type, struct net_buf_simple *ad)
{
    char payload[96];

    const char *range =
        (rssi > -55) ? "NEAR" :
        (rssi > -70) ? "MEDIUM" :
        (rssi > -85) ? "FAR" : "LOST";

    snprintf(payload, sizeof(payload),
             "{\"asset_id\":\"BEACON\",\"rssi\":%d,\"range\":\"%s\"}",
             rssi, range);

    struct mqtt_publish_param param = {
        .message.topic.qos = MQTT_QOS_0_AT_MOST_ONCE,
        .message.topic.topic.utf8 = MQTT_TOPIC,
        .message.topic.topic.size = strlen(MQTT_TOPIC),
        .message.payload.data = payload,
        .message.payload.len = strlen(payload),
        .message_id = msg_id++,
    };

    mqtt_publish(&client, &param);
}

void main(void)
{
    LOG_INF("ESP32 BLE → MQTT started");

    net_mgmt_init_event_callback(&net_cb,
        net_event_handler,
        NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&net_cb);

    LOG_INF("Waiting for Wi-Fi IP...");
    k_sem_take(&wifi_ready, K_FOREVER);

    mqtt_init();

    if (mqtt_connect(&client) != 0) {
        LOG_ERR("MQTT connect failed");
        return;
    }

    LOG_INF("MQTT connected");

    bt_enable(NULL);
    bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);

    while (1) {
        mqtt_input(&client);
        mqtt_live(&client);
        k_sleep(K_SECONDS(1));
    }
}
