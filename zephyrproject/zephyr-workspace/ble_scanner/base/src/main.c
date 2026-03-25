#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <string.h>
#include <stdio.h>

#define MAX_BEACONS 10
#define WINDOW_SIZE 5

struct beacon {
    char addr[BT_ADDR_LE_STR_LEN];
    int8_t rssi;
    int8_t history[WINDOW_SIZE];
    uint8_t idx;
    bool active;
};

static struct beacon beacons[MAX_BEACONS];

/* ---------------- FIND BEACON ---------------- */
static int find_beacon(const char *addr)
{
    for (int i = 0; i < MAX_BEACONS; i++) {
        if (beacons[i].active &&
            strcmp(beacons[i].addr, addr) == 0) {
            return i;
        }
    }
    return -1;
}

/* ---------------- FREE SLOT ---------------- */
static int get_free_slot(void)
{
    for (int i = 0; i < MAX_BEACONS; i++) {
        if (!beacons[i].active) return i;
    }
    return -1;
}

/* ---------------- RSSI FILTER ---------------- */
static int8_t smooth_rssi(struct beacon *b, int8_t rssi)
{
    b->history[b->idx++] = rssi;

    if (b->idx >= WINDOW_SIZE) {
        b->idx = 0;
    }

    int sum = 0;
    for (int i = 0; i < WINDOW_SIZE; i++) {
        sum += b->history[i];
    }

    return sum / WINDOW_SIZE;
}

/* ---------------- MANUFACTURER FILTER ---------------- */
static bool check_company_id(struct bt_data *data, void *user_data)
{
    bool *found = user_data;

    if (data->type == BT_DATA_MANUFACTURER_DATA &&
        data->data_len >= 2) {

        if (data->data[0] == 0xAF &&
            data->data[1] == 0xAF) {

            *found = true;
            return false; // stop parsing
        }
    }

    return true;
}

/* ---------------- SCAN CALLBACK ---------------- */
static void scan_cb(const struct bt_le_scan_recv_info *info,
                    struct net_buf_simple *ad)
{
    bool is_our_beacon = false;

    bt_data_parse(ad, check_company_id, &is_our_beacon);

    if (!is_our_beacon) {
        return;  // ignore other BLE devices
    }

    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(info->addr, addr, sizeof(addr));

    int idx = find_beacon(addr);

    if (idx == -1) {
        idx = get_free_slot();
        if (idx >= 0) {
            strcpy(beacons[idx].addr, addr);
            beacons[idx].active = true;
            beacons[idx].idx = 0;
        }
    }

    if (idx >= 0) {
        beacons[idx].rssi =
            smooth_rssi(&beacons[idx], info->rssi);

        /* 🔥 SEND TO GUI */
        printf("{\"mac\":\"%s\",\"rssi\":%d}\n",
               addr, beacons[idx].rssi);
    }
}

static struct bt_le_scan_cb scan_callbacks = {
    .recv = scan_cb,
};

/* ---------------- MAIN ---------------- */
int main(void)
{
    int err;

    printf("Base Station Started...\n");

    err = bt_enable(NULL);
    if (err) {
        printf("Bluetooth init failed (%d)\n", err);
        return 0;
    }

    bt_le_scan_cb_register(&scan_callbacks);

    struct bt_le_scan_param scan_param = {
        .type = BT_LE_SCAN_TYPE_PASSIVE,
        .options = BT_LE_SCAN_OPT_NONE,
        .interval = 0x0060,
        .window = 0x0030,
    };

    err = bt_le_scan_start(&scan_param, NULL);
    if (err) {
        printf("Scan start failed (%d)\n", err);
        return 0;
    }

    printf("Scanning for beacons...\n");

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
