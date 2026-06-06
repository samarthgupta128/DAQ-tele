import sys
import struct
import csv
import time
from datetime import datetime
import serial
import serial.tools.list_ports
from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout,
                             QHBoxLayout, QLabel, QPushButton, QComboBox,
                             QGridLayout, QTextEdit, QFrame)
from PyQt6.QtCore import Qt, QThread, pyqtSignal
from PyQt6.QtGui import QFont, QPixmap, QColor, QPalette

# --- BACKGROUND SERIAL THREAD ---
class SerialWorker(QThread):
    data_received = pyqtSignal(dict)
    log_msg = pyqtSignal(str)

    def __init__(self, port, baudrate):
        super().__init__()
        self.port = port
        self.baudrate = baudrate
        self.is_running = True
        self.is_logging = False
        self.csv_file = None
        self.csv_writer = None
        
        # Exact match to your C-Struct (36 bytes total)
        # I=uint32, h=int16, H=uint16
        self.struct_format = '<IhhhhHHHHHHhhhhhh'
        self.packet_size = struct.calcsize(self.struct_format)

    def run(self):
        try:
            with serial.Serial(self.port, self.baudrate, timeout=1) as ser:
                self.log_msg.emit(f"Connected to {self.port} at {self.baudrate} baud.")
                
                while self.is_running:
                    if ser.in_waiting > 0:
                        # 1. Read a full line until the \r\n boundary
                        line = ser.readline().decode('utf-8', errors='ignore').strip()
                        
                        expected_chars = self.packet_size * 2
                        
                        # 2. THE FIX: If the line has extra text (like timestamps), 
                        # just chop off everything except the last 72 hex characters.
                        if len(line) >= expected_chars:
                            line = line[-expected_chars:]
                        
                        # 3. Proceed only if we have exactly our 36-byte hex payload
                        if len(line) == expected_chars:
                            try:
                                # Convert Hex string back to raw binary bytes
                                raw_bytes = bytes.fromhex(line)
                                
                                # Unpack the bytes into your C-Struct
                                unpacked = struct.unpack(self.struct_format, raw_bytes)
                                
                                # Reconstruct values based on STM32 multipliers
                                data = {
                                    "Time_ms": unpacked[0],
                                    "RPM": unpacked[1],
                                    "MotorTemp": unpacked[2] / 10.0,
                                    "MCUTemp": unpacked[3] / 10.0,
                                    "Iq_Actual": unpacked[4],
                                    "Pot1": unpacked[5] / 100.0,
                                    "Pot2": unpacked[6] / 100.0,
                                    "Pot3": unpacked[7] / 100.0,
                                    "Pot4": unpacked[8] / 100.0,
                                    "Pot5": unpacked[9] / 100.0,
                                    "BrakeVolt": unpacked[10] / 100.0,
                                    "Ax": unpacked[11], "Ay": unpacked[12], "Az": unpacked[13],
                                    "RollRate": unpacked[14], "PitchRate": unpacked[15], "YawRate": unpacked[16]
                                }
                                
                                # Send data to the GUI gauges
                                self.data_received.emit(data)

                                # Write to the CSV file if recording is active
                                if self.is_logging and self.csv_writer:
                                    self.csv_writer.writerow(data.values())
                                    
                            except Exception as e:
                                # If a corrupted hex string sneaks through, ignore it 
                                # and keep the GUI running smoothly.
                                pass 

        except Exception as e:
            self.log_msg.emit(f"Serial Error: {str(e)}")
    def start_logging(self):
        filename = f"log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
        self.csv_file = open(filename, mode='w', newline='')
        self.csv_writer = csv.writer(self.csv_file)
        # Write headers
        headers = ["Time(ms)", "RPM", "MotorTemp", "MCUTemp", "Iq_Actual", "Pot1", "Pot2", "Pot3", "Pot4", "Pot5", "BrakeVolt", "Ax", "Ay", "Az", "RollRate", "PitchRate", "YawRate"]
        self.csv_writer.writerow(headers)
        self.is_logging = True
        self.log_msg.emit(f"Started logging to {filename}")

    def stop_logging(self):
        self.is_logging = False
        if self.csv_file:
            self.csv_file.close()
        self.log_msg.emit("Stopped logging to CSV.")

    def stop(self):
        self.is_running = False
        self.stop_logging()
        self.wait()


# --- MAIN GUI WINDOW ---
class DAQWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Roorkee Motorsports - Telemetry DAQ")
        self.setGeometry(100, 100, 1200, 800)
        self.apply_dark_theme()

        self.serial_thread = None
        self.labels = {}

        # Main Layout
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)

        # 1. Header Section
        header_layout = QHBoxLayout()
        
        self.logo_label = QLabel()
        self.logo_label.setFixedSize(197, 136)
        # self.logo_label.setStyleSheet("border: 2px dashed #555; background-color: #222;")
        self.logo_label.setText("LOGO\nHERE")
        self.logo_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        
        # HOW TO ADD LOGO: 
        # 1. Place your logo image (e.g., 'roorkee_logo.png') in the same folder as this script.
        # 2. Uncomment the two lines below and change the filename.
        pixmap = QPixmap('iitrms_logo.jpeg').scaled(197, 136, Qt.AspectRatioMode.KeepAspectRatio, Qt.TransformationMode.SmoothTransformation)
        self.logo_label.setPixmap(pixmap)

        title_label = QLabel("IIT ROORKEE MOTORSPORTS")
        title_label.setFont(QFont("Arial", 36, QFont.Weight.Bold))
        title_label.setStyleSheet("color: #E0E0E0;")

        header_layout.addWidget(self.logo_label)
        header_layout.addSpacing(20)
        header_layout.addWidget(title_label)
        header_layout.addStretch()
        main_layout.addLayout(header_layout)

        # 2. Control Panel
        control_layout = QHBoxLayout()
        
        self.port_combo = QComboBox()
        self.refresh_ports()
        
        self.baud_combo = QComboBox()
        self.baud_combo.addItems(["9600", "115200", "500000"])
        self.baud_combo.setCurrentText("115200")

        self.btn_refresh = QPushButton("Refresh Ports")
        self.btn_refresh.clicked.connect(self.refresh_ports)

        self.btn_connect = QPushButton("CONNECT")
        self.btn_connect.setStyleSheet("background-color: #2E7D32; font-weight: bold;")
        self.btn_connect.clicked.connect(self.toggle_connection)

        self.btn_record = QPushButton("START RECORDING (CSV)")
        self.btn_record.setStyleSheet("background-color: #C62828; font-weight: bold;")
        self.btn_record.setEnabled(False)
        self.btn_record.clicked.connect(self.toggle_recording)

        control_layout.addWidget(QLabel("Port:"))
        control_layout.addWidget(self.port_combo)
        control_layout.addWidget(self.btn_refresh)
        control_layout.addWidget(QLabel("Baud:"))
        control_layout.addWidget(self.baud_combo)
        control_layout.addWidget(self.btn_connect)
        control_layout.addStretch()
        control_layout.addWidget(self.btn_record)
        
        main_layout.addLayout(control_layout)

        # 3. Telemetry Dashboard (Grid)
        grid_layout = QGridLayout()
        main_layout.addLayout(grid_layout)

        # Define the layout of gauges
        gauge_config = [
            ("Time(ms)", 0, 0), ("RPM", 0, 1), ("Iq_Actual", 0, 2), ("BrakeVolt", 0, 3),
            ("MotorTemp", 1, 0), ("MCUTemp", 1, 1), ("Ax", 1, 2), ("RollRate", 1, 3),
            ("Pot1", 2, 0), ("Pot2", 2, 1), ("Ay", 2, 2), ("PitchRate", 2, 3),
            ("Pot3", 3, 0), ("Pot4", 3, 1), ("Az", 3, 2), ("YawRate", 3, 3),
            ("Pot5", 4, 0)
        ]

        for name, row, col in gauge_config:
            box = QFrame()
            box.setStyleSheet("background-color: #1E1E1E; border-radius: 8px; border: 1px solid #333;")
            box_layout = QVBoxLayout(box)
            
            title = QLabel(name)
            title.setStyleSheet("color: #888; font-size: 14px; border: none;")
            title.setAlignment(Qt.AlignmentFlag.AlignCenter)
            
            value = QLabel("0.00")
            value.setFont(QFont("Monospace", 28, QFont.Weight.Bold))
            value.setStyleSheet("color: #00E676; border: none;") # Neon Green Text
            value.setAlignment(Qt.AlignmentFlag.AlignCenter)
            
            self.labels[name] = value
            box_layout.addWidget(title)
            box_layout.addWidget(value)
            grid_layout.addWidget(box, row, col)

        # 4. Console Log
        self.console = QTextEdit()
        self.console.setReadOnly(True)
        self.console.setMaximumHeight(150)
        self.console.setStyleSheet("background-color: #121212; color: #BBB; font-family: monospace;")
        main_layout.addWidget(self.console)

    def apply_dark_theme(self):
        palette = QPalette()
        palette.setColor(QPalette.ColorRole.Window, QColor(40, 40, 40))
        palette.setColor(QPalette.ColorRole.WindowText, Qt.GlobalColor.white)
        palette.setColor(QPalette.ColorRole.Button, QColor(60, 60, 60))
        palette.setColor(QPalette.ColorRole.ButtonText, Qt.GlobalColor.white)
        QApplication.setPalette(palette)

    def refresh_ports(self):
        self.port_combo.clear()
        ports = serial.tools.list_ports.comports()
        for p in ports:
            self.port_combo.addItem(p.device)

    def log(self, msg):
        self.console.append(f"[{datetime.now().strftime('%H:%M:%S')}] {msg}")

    def toggle_connection(self):
        if self.serial_thread is None or not self.serial_thread.isRunning():
            # Connect
            port = self.port_combo.currentText()
            baud = int(self.baud_combo.currentText())
            if not port:
                self.log("No port selected!")
                return

            self.serial_thread = SerialWorker(port, baud)
            self.serial_thread.data_received.connect(self.update_dashboard)
            self.serial_thread.log_msg.connect(self.log)
            self.serial_thread.start()

            self.btn_connect.setText("DISCONNECT")
            self.btn_connect.setStyleSheet("background-color: #D84315; font-weight: bold;")
            self.btn_record.setEnabled(True)
        else:
            # Disconnect
            self.serial_thread.stop()
            self.serial_thread = None
            self.btn_connect.setText("CONNECT")
            self.btn_connect.setStyleSheet("background-color: #2E7D32; font-weight: bold;")
            self.btn_record.setText("START RECORDING (CSV)")
            self.btn_record.setStyleSheet("background-color: #C62828; font-weight: bold;")
            self.btn_record.setEnabled(False)

    def toggle_recording(self):
        if self.serial_thread and self.serial_thread.is_logging:
            self.serial_thread.stop_logging()
            self.btn_record.setText("START RECORDING (CSV)")
            self.btn_record.setStyleSheet("background-color: #C62828; font-weight: bold;")
        elif self.serial_thread:
            self.serial_thread.start_logging()
            self.btn_record.setText("STOP RECORDING")
            self.btn_record.setStyleSheet("background-color: #555555; font-weight: bold;")

    def update_dashboard(self, data):
        for key, val in data.items():
            if key in self.labels:
                # Format specific values
                if isinstance(val, float):
                    self.labels[key].setText(f"{val:.2f}")
                else:
                    self.labels[key].setText(str(val))


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = DAQWindow()
    window.show()
    sys.exit(app.exec())