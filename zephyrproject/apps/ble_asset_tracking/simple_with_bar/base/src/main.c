#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#define RSSI_WINDOW 5
#define LOST_TIMEOUT_MS 5000

/* Beacon UUID */
static const uint8_t target_uuid[16] = {
    0x12, 0x34, 0x56, 0x78,
    0x90, 0xAB,
    0xCD, 0xEF,
    0x12, 0x34,
    0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF
};

static int8_t rssi_buf[RSSI_WINDOW];
static uint8_t rssi_idx;
static bool rssi_full;

static int8_t avg_rssi = -127;
static int64_t last_seen;
static char last_zone[8] = "NONE";

/* ---------- RSSI helpers ---------- */

static int8_t calc_avg_rssi(void)
{
    int sum = 0;
    int count = rssi_full ? RSSI_WINDOW : rssi_idx;

    for (int i = 0; i < count; i++) {
        sum += rssi_buf[i];
    }
    return sum / count;
}

static const char *zone_from_rssi(int8_t rssi)
{
    if (rssi >= -55) return "NEAR";
    if (rssi >= -70) return "MEDIUM";
    if (rssi >= -85) return "FAR";
    return "LOST";
}

static int bars_from_rssi(int8_t rssi)
{
    if (rssi >= -55) return 10;
    if (rssi >= -65) return 7;
    if (rssi >= -75) return 5;
    if (rssi >= -85) return 3;
    return 0;
}

static void print_bar(int bars)
{
    printk("[");
    for (int i = 0; i < 10; i++) {
        printk(i < bars ? "#" : "-");
    }
    printk("]");
}

/* ---------- BLE callbacks ---------- */

static bool ad_parse_cb(struct bt_data *data, void *user_data)
{
    int8_t rssi = *(int8_t *)user_data;

    if (data->type == BT_DATA_UUID128_ALL &&
        data->data_len == 16 &&
        memcmp(data->data, target_uuid, 16) == 0) {

        rssi_buf[rssi_idx++] = rssi;
        if (rssi_idx >= RSSI_WINDOW) {
            rssi_idx = 0;
            rssi_full = true;
        }

        avg_rssi = calc_avg_rssi();
        last_seen = k_uptime_get();

        const char *zone = zone_from_rssi(avg_rssi);

        /* ENTER / EXIT detection */
        if (strcmp(zone, last_zone) != 0) {
            printk("EVENT: %s → %s\n", last_zone, zone);
            strcpy(last_zone, zone);
        }

        printk("AVG RSSI: %d dBm | %-6s | ", avg_rssi, zone);
        print_bar(bars_from_rssi(avg_rssi));
        printk("\n");

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

/* ---------- MAIN ---------- */

int main(void)
{
    printk("BLE Asset Tracker (Upgraded)\n");

    if (bt_enable(NULL)) {
        printk("Bluetooth init failed\n");
        return 0;
    }

    bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);

    while (1) {
        if (k_uptime_get() - last_seen > LOST_TIMEOUT_MS) {
            if (strcmp(last_zone, "LOST") != 0) {
                printk("EVENT: %s → LOST\n", last_zone);
                strcpy(last_zone, "LOST");
            }
            printk("AVG RSSI: --     | LOST   | [----------]\n");
            last_seen = k_uptime_get();
        }

        k_sleep(K_SECONDS(1));
    }
}
