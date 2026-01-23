#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/sys/printk.h>
#include <string.h>

/* ================= WIFI CONFIG ================= */
#define WIFI_SSID       "Cricclubs"
#define WIFI_PSK        "cricclubs"

/* ================= MQTT CONFIG ================= */
#define MQTT_BROKER_IP  "10.53.234.9"
#define MQTT_PORT       1883
#define MQTT_TOPIC      "asset/rssi"

/* ================= BEACON UUID ================= */
static const uint8_t target_uuid[16] = {
    0x12,0x34,0x56,0x78,0x90,0xAB,0xCD,0xEF,
    0x12,0x34,0x56,0x78,0x90,0xAB,0xCD,0xEF
};

/* ================= GLOBAL STATE ================= */
static bool mqtt_ready;

/* ================= MQTT ================= */
static struct mqtt_client mqtt_client;
static struct sockaddr_storage broker;

/* RSSI → ZONE */
static const char *zone_from_rssi(int8_t rssi)
{
    if (rssi >= -55) return "NEAR";
    if (rssi >= -70) return "MEDIUM";
    if (rssi >= -85) return "FAR";
    return "LOST";
}

static void mqtt_event_handler(struct mqtt_client *client,
                               const struct mqtt_evt *evt)
{
    if (evt->type == MQTT_EVT_CONNACK) {
        if (evt->result == 0) {
            mqtt_ready = true;
            printk("MQTT CONNECTED\n");
        } else {
            printk("MQTT CONNECT FAILED (%d)\n", evt->result);
        }
    }
}

static void mqtt_connect_to_broker(void)
{
    struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;

    mqtt_client_init(&mqtt_client);

    broker4->sin_family = AF_INET;
    broker4->sin_port = htons(MQTT_PORT);
    net_addr_pton(AF_INET, MQTT_BROKER_IP, &broker4->sin_addr);

    mqtt_client.broker = &broker;
    mqtt_client.evt_cb = mqtt_event_handler;
    mqtt_client.client_id.utf8 = (uint8_t *)"esp32_asset_tracker";
    mqtt_client.client_id.size = strlen("esp32_asset_tracker");
    mqtt_client.protocol_version = MQTT_VERSION_3_1_1;
    mqtt_client.transport.type = MQTT_TRANSPORT_NON_SECURE;

    printk("Connecting to MQTT broker...\n");
    mqtt_connect(&mqtt_client);
}

static void mqtt_publish_rssi(int8_t rssi)
{
    if (!mqtt_ready) {
        return;
    }

    char payload[64];
    snprintf(payload, sizeof(payload),
             "{\"rssi\":%d,\"zone\":\"%s\"}",
             rssi, zone_from_rssi(rssi));

    struct mqtt_publish_param param = {
        .message.topic.qos = MQTT_QOS_0_AT_MOST_ONCE,
        .message.topic.topic.utf8 = (uint8_t *)MQTT_TOPIC,
        .message.topic.topic.size = strlen(MQTT_TOPIC),
        .message.payload.data = payload,
        .message.payload.len = strlen(payload),
        .message_id = sys_rand32_get(),
    };

    mqtt_publish(&mqtt_client, &param);
}

/* ================= BLE ================= */
static bool ad_parse_cb(struct bt_data *data, void *user_data)
{
    int8_t rssi = *(int8_t *)user_data;

    if (data->type == BT_DATA_UUID128_ALL &&
        data->data_len == 16 &&
        memcmp(data->data, target_uuid, 16) == 0) {

        printk("RSSI=%d dBm | %s\n", rssi, zone_from_rssi(rssi));
        mqtt_publish_rssi(rssi);
        return false;
    }
    return true;
}

static void device_found(const bt_addr_le_t *addr,
                         int8_t rssi,
                         uint8_t type,
                         struct net_buf_simple *ad)
{
    bt_data_parse(ad, ad_parse_cb, &rssi);
}

/* ================= NETWORK EVENTS ================= */
static void net_event_handler(struct net_mgmt_event_callback *cb,
                              uint32_t event,
                              struct net_if *iface)
{
    if (event == NET_EVENT_WIFI_CONNECT_RESULT) {
        struct wifi_status *status = cb->info;
        printk("Wi-Fi connect result: %s\n",
               status->status ? "FAIL" : "SUCCESS");
    }

    if (event == NET_EVENT_IPV4_ADDR_ADD) {
        struct net_if_addr *ifaddr = cb->info;
        char ip[NET_IPV4_ADDR_LEN];

        printk("IP ACQUIRED: %s\n",
               net_addr_ntop(AF_INET,
                             &ifaddr->address.in_addr,
                             ip, sizeof(ip)));

        mqtt_connect_to_broker();
    }
}

static struct net_mgmt_event_callback net_cb;

/* ================= WIFI ================= */
static void wifi_connect(void)
{
    struct net_if *iface = net_if_get_default();

    struct wifi_connect_req_params params = {
        .ssid = WIFI_SSID,
        .ssid_length = strlen(WIFI_SSID),
        .psk = WIFI_PSK,
        .psk_length = strlen(WIFI_PSK),
        .security = WIFI_SECURITY_TYPE_PSK,
        .channel = WIFI_CHANNEL_ANY,
    };

    printk("Connecting to Wi-Fi...\n");
    net_mgmt(NET_REQUEST_WIFI_CONNECT,
             iface, &params, sizeof(params));
}

/* ================= MAIN ================= */
int main(void)
{
    printk("BLE → MQTT Asset Tracker (FINAL)\n");

    net_mgmt_init_event_callback(
        &net_cb,
        net_event_handler,
        NET_EVENT_WIFI_CONNECT_RESULT |
        NET_EVENT_IPV4_ADDR_ADD
    );
    net_mgmt_add_event_callback(&net_cb);

    wifi_connect();

    bt_enable(NULL);
    bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);

    while (1) {
        if (mqtt_ready) {
            mqtt_input(&mqtt_client);
            mqtt_live(&mqtt_client);
        }
        k_sleep(K_SECONDS(1));
    }
}
