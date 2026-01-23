#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <string.h>

/* ================= iBeacon UUID ================= */
static const uint8_t target_uuid[16] = {
    0x12,0x34,0x56,0x78,
    0x12,0x34,
    0x12,0x34,
    0x12,0x34,
    0x12,0x34,0x56,0x78,0x90,0xab
};

/* ================= RSSI FILTER ================= */
#define EMA_ALPHA 0.2f

static float rssi_filtered = -100.0f;
static bool first_sample = true;

static int smooth_rssi(int rssi)
{
    if (first_sample) {
        rssi_filtered = rssi;
        first_sample = false;
    } else {
        rssi_filtered = EMA_ALPHA * rssi +
                        (1.0f - EMA_ALPHA) * rssi_filtered;
    }
    return (int)rssi_filtered;
}

/* ================= ZONE ================= */
static const char *zone(int rssi)
{
    if (rssi > -55) return "NEAR";
    if (rssi > -70) return "MEDIUM";
    if (rssi > -85) return "FAR";
    return "LOST";
}

/* ================= BAR GRAPH ================= */
#define BAR_WIDTH 20
#define RSSI_MIN  (-100)
#define RSSI_MAX  (-40)

static void print_bar(int rssi)
{
    int level;

    if (rssi < RSSI_MIN) rssi = RSSI_MIN;
    if (rssi > RSSI_MAX) rssi = RSSI_MAX;

    level = (rssi - RSSI_MIN) * BAR_WIDTH / (RSSI_MAX - RSSI_MIN);

    for (int i = 0; i < BAR_WIDTH; i++) {
        if (i < level) {
            printk("█");
        } else {
            printk("░");
        }
    }
}

/* ================= AD PARSE ================= */
static bool parse_ad(struct bt_data *data, void *user_data)
{
    if (data->type != BT_DATA_MANUFACTURER_DATA) {
        return true;
    }

    if (data->data_len < 25) {
        return true;
    }

    const uint8_t *p = data->data;

    /* Apple ID + iBeacon prefix */
    if (p[0] != 0x4C || p[1] != 0x00 || p[2] != 0x02 || p[3] != 0x15) {
        return true;
    }

    if (memcmp(&p[4], target_uuid, 16) == 0) {
        *(bool *)user_data = true;
        return false;
    }

    return true;
}

/* ================= SCAN CALLBACK ================= */
static void device_found(const bt_addr_le_t *addr,
                         int8_t rssi,
                         uint8_t type,
                         struct net_buf_simple *ad)
{
    bool found = false;

    bt_data_parse(ad, parse_ad, &found);

    if (found) {
        int smooth = smooth_rssi(rssi);

        printk("RSSI raw=%d | smooth=%d | %-6s | ",
               rssi, smooth, zone(smooth));

        print_bar(smooth);
        printk("\n");
    }
}

/* ================= MAIN ================= */
int main(void)
{
    int err;

    printk("ESP32 BLE Mobile Beacon Tracker (RSSI + BAR GRAPH)\n");

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (%d)\n", err);
        return 0;
    }

    printk("Bluetooth ready. Start phone beacon...\n");

    err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
    if (err) {
        printk("Scan failed (%d)\n", err);
        return 0;
    }

    while (1) {
        k_sleep(K_SECONDS(1));
    }
}
