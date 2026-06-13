#include <Arduino.h>

// Structure aligned to 1-byte boundaries to prevent compiler padding
struct __attribute__((packed)) TelemetryPacket {
    uint8_t sync1 = 0xAA;       // Start of Frame marker 1
    uint8_t sync2 = 0xBB;       // Start of Frame marker 2
    uint16_t packet_id;         // Incremental frame counter
    
    uint16_t rpm;
    float mcu_temp;
    float motor_temp;
    float ax, ay, az;
    float yaw, pitch, roll;
    
    uint16_t pot1, pot2, pot3, pot4, pot5;
    uint8_t checksum;           // XOR checksum
};

TelemetryPacket txData;
uint16_t frameCounter = 0;

void setup() {
    // Standard USB Serial for Debugging
    Serial.begin(9600);
    
    // Hardware UART2 for STM32 Communication (Baud: 115200, RX: Pin 16, TX: Pin 17)
    Serial2.begin(9600, SERIAL_8N1, 16, 17);
    
    Serial.println("ESP32 UART Telemetry Transmitter Initialized.");
}

void loop() {
    // 1. Generate Dummy/Random Vehicle Data
    txData.packet_id = frameCounter++;
    txData.rpm = random(0, 8000);                      
    txData.mcu_temp = random(250, 750) / 10.0;         
    txData.motor_temp = random(300, 1100) / 10.0;      
    
    txData.ax = (random(-400, 400) / 100.0);
    txData.ay = (random(-400, 400) / 100.0);
    txData.az = (random(-400, 400) / 100.0);
    
    txData.yaw = random(0, 3600) / 10.0;
    txData.pitch = random(-900, 900) / 10.0;
    txData.roll = random(-1800, 1800) / 10.0;
    
    txData.pot1 = random(0, 4096);
    txData.pot2 = random(0, 4096);
    txData.pot3 = random(0, 4096);
    txData.pot4 = random(0, 4096);
    txData.pot5 = random(0, 4096);
    
    // 2. Calculate XOR Checksum over the data payload
    uint8_t* bytePtr = (uint8_t*)&txData;
    uint8_t calculatedXOR = 0;
    for (size_t i = 0; i < sizeof(TelemetryPacket) - 1; i++) {
        calculatedXOR ^= bytePtr[i];
    }
    txData.checksum = calculatedXOR;

    // 3. Transmit the 49-byte packet over UART
    Serial2.write(bytePtr, sizeof(TelemetryPacket));

    // Debug Print to Serial Monitor
    Serial.printf("Sent Packet #%d | RPM: %d | Ax: %.2f\n", 
                  txData.packet_id, txData.rpm, txData.ax);
                  
    delay(200); // Transmit payload at ~20Hz rate
}