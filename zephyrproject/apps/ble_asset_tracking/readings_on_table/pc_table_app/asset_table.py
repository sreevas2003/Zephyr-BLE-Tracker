import json
import threading
import tkinter as tk
from tkinter import ttk
import paho.mqtt.client as mqtt

BROKER = "127.0.0.1"
TOPIC = "asset/table"

root = tk.Tk()
root.title("BLE Asset Tracker")

table = ttk.Treeview(root, columns=("RSSI", "Range"), show="headings")
table.heading("RSSI", text="RSSI (dBm)")
table.heading("Range", text="Range")
table.pack(fill=tk.BOTH, expand=True)

rows = {}

def on_message(client, userdata, msg):
    data = json.loads(msg.payload.decode())
    asset = data["asset_id"]
    rssi = data["rssi"]
    rng = data["range"]

    if asset in rows:
        table.item(rows[asset], values=(rssi, rng))
    else:
        rows[asset] = table.insert("", "end", values=(rssi, rng))

def mqtt_thread():
    client = mqtt.Client(protocol=mqtt.MQTTv311)
    client.on_message = on_message
    client.connect(BROKER, 1883, 60)
    client.subscribe(TOPIC)
    client.loop_forever()

threading.Thread(target=mqtt_thread, daemon=True).start()
root.mainloop()
