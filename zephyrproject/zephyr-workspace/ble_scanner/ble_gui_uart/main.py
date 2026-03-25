import sys
import time
import json
import os
import serial

from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QTableWidget,
    QTableWidgetItem, QVBoxLayout, QWidget, QLabel,
    QMessageBox, QHeaderView, QPushButton, QInputDialog
)
from PyQt6.QtCore import Qt, QTimer, QThread, pyqtSignal
from PyQt6.QtGui import QColor, QBrush, QFont

# ---------------------------------------------------------
# CONFIGURATION
# ---------------------------------------------------------
SERIAL_PORT = "/dev/ttyACM1"
BAUDRATE = 115200

DEVICE_TIMEOUT = 10.0
RANGE_THRESHOLD = -100
BEACON_FILE = "beacons.json"

# ---------------------------------------------------------
# SERIAL WORKER
# ---------------------------------------------------------
class SerialWorker(QThread):
    data_received = pyqtSignal(str, int)

    def __init__(self):
        super().__init__()
        self.running = True

    def run(self):
        ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)

        while self.running:
            try:
                line = ser.readline().decode().strip()
                if line:
                    data = json.loads(line)
                    mac = data.get("mac", "").split(" ")[0].strip().upper()
                    rssi = int(data.get("rssi", -127))
                    self.data_received.emit(mac, rssi)
            except:
                pass

    def stop(self):
        self.running = False

# ---------------------------------------------------------
# MAIN GUI
# ---------------------------------------------------------
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()

        self.setWindowTitle("Wireless Asset Tracking Dashboard (UART)")
        self.resize(1250, 600)

        # ---------- Load Names ----------
        self.beacon_names = self.load_beacon_names()

        # ---------- Title ----------
        self.title_label = QLabel("WIRELESS TRACKING SYSTEM (UART)")
        self.title_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.title_label.setStyleSheet("""
            QLabel {
                font-size: 22px;
                font-weight: 600;
                padding: 14px;
                color: #ffffff;
                background-color: #2c3e50;
            }
        """)

        # ---------- Table ----------
        self.table = QTableWidget(0, 8)
        self.table.setHorizontalHeaderLabels(
            ["Asset Name", "MAC Address", "RSSI", "Distance (m)", 
             "Range Status", "Availability", "Last Seen", "Action"]
        )

        self.table.verticalHeader().setVisible(False)
        self.table.setShowGrid(False)
        self.table.setSelectionMode(self.table.SelectionMode.NoSelection)
        self.table.setEditTriggers(self.table.EditTrigger.NoEditTriggers)
        self.table.verticalHeader().setDefaultSectionSize(50)

        self.table.setColumnWidth(0, 220)
        self.table.setColumnWidth(1, 190)
        self.table.setColumnWidth(2, 80)
        self.table.setColumnWidth(3, 120)
        self.table.setColumnWidth(4, 160)
        self.table.setColumnWidth(5, 160)
        self.table.setColumnWidth(6, 120)

        self.table.horizontalHeader().setStretchLastSection(True)

        self.table.setStyleSheet("""
        QTableWidget {
            background-color: #ecf0f1;
            alternate-background-color: #bdc3c7;
            color: #2c3e50;
            font-size: 14px;
            font-weight: 500;
        }
        QHeaderView::section {
            background-color: #34495e;
            color: #ffffff;
            padding: 10px;
            border: none;
            font-size: 14px;
            font-weight: 600;
        }
        QTableWidget::item {
            padding: 8px;
        }
        """)
        self.table.setAlternatingRowColors(True)

        layout = QVBoxLayout()
        layout.setSpacing(0)
        layout.addWidget(self.title_label)
        layout.addWidget(self.table)

        container = QWidget()
        container.setLayout(layout)
        self.setCentralWidget(container)

        # Fonts
        self.mac_font = QFont("Monospace", 11)
        self.rssi_font = QFont()
        self.rssi_font.setPointSize(12)
        self.rssi_font.setBold(True)

        # State
        self.mac_to_row = {}
        self.last_seen_time = {}
        self.current_rssi = {}
        self.alert_triggered = {}

        # SERIAL WORKER
        self.worker = SerialWorker()
        self.worker.data_received.connect(self.update_beacon)
        self.worker.start()

        # Timer
        self.timer = QTimer()
        self.timer.timeout.connect(self.refresh_status)
        self.timer.start(1000)

    # ---------------------------------------------------------
    def calculate_distance(self, rssi, tx_power=-59, n=2):
        return round(10 ** ((tx_power - rssi) / (10 * n)), 2)

    # ---------------------------------------------------------
    def load_beacon_names(self):
        if not os.path.exists(BEACON_FILE):
            return {}
        try:
            with open(BEACON_FILE, 'r') as f:
                return json.load(f)
        except:
            return {}

    def save_beacon_names(self):
        with open(BEACON_FILE, 'w') as f:
            json.dump(self.beacon_names, f, indent=4)

    # ---------------------------------------------------------
    def add_new_beacon_row(self, mac):
        row = self.table.rowCount()
        self.table.insertRow(row)

        name = self.beacon_names.get(mac, "Unknown Device")
        self.table.setItem(row, 0, QTableWidgetItem(name))

        mac_item = QTableWidgetItem(mac)
        mac_item.setFont(self.mac_font)
        mac_item.setForeground(QBrush(QColor("#7f8c8d")))
        self.table.setItem(row, 1, mac_item)

        for col in range(2, 7):
            self.table.setItem(row, col, QTableWidgetItem("--"))

        # Button
        btn = QPushButton("Edit")
        btn.clicked.connect(lambda: self.edit_device_name(mac))
        self.table.setCellWidget(row, 7, btn)

        self.mac_to_row[mac] = row
        self.alert_triggered[mac] = True

    # ---------------------------------------------------------
    def edit_device_name(self, mac):
        name, ok = QInputDialog.getText(self, "Edit Name", f"{mac}")
        if ok and name:
            self.beacon_names[mac] = name
            self.table.item(self.mac_to_row[mac], 0).setText(name)
            self.save_beacon_names()

    # ---------------------------------------------------------
    def update_beacon(self, mac, rssi):
        if mac not in self.mac_to_row:
            self.add_new_beacon_row(mac)

        row = self.mac_to_row[mac]
        self.last_seen_time[mac] = time.time()
        self.current_rssi[mac] = rssi

        # RSSI
        rssi_item = self.table.item(row, 2)
        rssi_item.setText(str(rssi))
        rssi_item.setFont(self.rssi_font)

        if rssi > -60:
            rssi_item.setBackground(QBrush(QColor("#2ecc71")))
        elif rssi > -75:
            rssi_item.setBackground(QBrush(QColor("#f1c40f")))
        else:
            rssi_item.setBackground(QBrush(QColor("#e74c3c")))

        # Distance
        dist = self.calculate_distance(rssi)
        self.table.item(row, 3).setText(f"{dist} m")

        # Range
        range_item = self.table.item(row, 4)
        if rssi >= RANGE_THRESHOLD:
            range_item.setText("WITHIN RANGE")
            range_item.setForeground(QBrush(QColor("#27ae60")))
        else:
            range_item.setText("OUT OF RANGE")
            range_item.setForeground(QBrush(QColor("#c0392b")))

    # ---------------------------------------------------------
    def refresh_status(self):
        now = time.time()

        for mac, row in self.mac_to_row.items():
            last = self.last_seen_time.get(mac, 0)
            elapsed = now - last

            avail_item = self.table.item(row, 5)

            if elapsed > DEVICE_TIMEOUT:
                avail_item.setText("NOT AVAILABLE")
                avail_item.setBackground(QBrush(QColor("#7f8c8d")))
            else:
                avail_item.setText("ACTIVE")
                avail_item.setBackground(QBrush(QColor("#27ae60")))

            self.table.item(row, 6).setText(f"{int(elapsed)}s ago")

    # ---------------------------------------------------------
    def closeEvent(self, event):
        self.worker.stop()
        event.accept()

# ---------------------------------------------------------
if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())
