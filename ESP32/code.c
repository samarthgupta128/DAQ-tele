#define RX_PIN 16
#define TX_PIN 17

// 1. Define the structural layout of your telemetry data (Exactly 68 bytes)
struct __attribute__((packed)) TelemetryPacket {
    uint32_t time_ms;
    uint32_t rpm;
    float motor_temp;
    float mcu_temp;
    float iq_actual;
    float pot1, pot2, pot3, pot4, pot5;
    float brake_volt;
    float ax, ay, az;
    float roll_rate, pitch_rate, yaw_rate;
};

TelemetryPacket telemetry;

// Helper function to generate random floats within a range
float getRandomFloat(float minVal, float maxVal) {
    return minVal + ((float)random(0, 10000) / 10000.0) * (maxVal - minVal);
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  
  // Seed the random number generator using an unconnected analog pin
  randomSeed(analogRead(34)); 
  
  delay(1000);
  Serial.println("\n--- ESP32 Telemetry Transmitter Started ---");
}

void loop() {
  // 2. Populate the struct with simulated, dynamic telemetry data
  
  // Keep time sequential so your graphs/logs actually make sense
  telemetry.time_ms    = millis(); 
  
  // Dynamic ranges based on typical EV / Motor controller setups
  telemetry.rpm        = random(0, 4501);          // 0 to 4500 RPM
  telemetry.motor_temp = getRandomFloat(25.0, 85.0); // 25°C to 85°C
  telemetry.mcu_temp   = getRandomFloat(30.0, 60.0); // 30°C to 60°C
  telemetry.iq_actual  = getRandomFloat(-5.0, 35.0); // -5A to 35A (current)
  
  // Potentiometers (Simulating 0V to 3.3V ADC inputs)
  telemetry.pot1       = getRandomFloat(0.0, 3.3);
  telemetry.pot2       = getRandomFloat(0.0, 3.3);
  telemetry.pot3       = getRandomFloat(0.0, 3.3);
  telemetry.pot4       = getRandomFloat(0.0, 3.3);
  telemetry.pot5       = getRandomFloat(0.0, 3.3);
  
  telemetry.brake_volt = getRandomFloat(0.0, 5.0);   // 0V to 5V brake sensor
  
  // IMU Data: Accelerometer (in Gs, e.g., -2G to +2G)
  telemetry.ax         = getRandomFloat(-2.0, 2.0);
  telemetry.ay         = getRandomFloat(-2.0, 2.0);
  telemetry.az         = getRandomFloat(-2.0, 2.0);
  
  // IMU Data: Gyroscope angular rates (in degrees/second)
  telemetry.roll_rate  = getRandomFloat(-90.0, 90.0);
  telemetry.pitch_rate = getRandomFloat(-90.0, 90.0);
  telemetry.yaw_rate   = getRandomFloat(-180.0, 180.0);

  // 3. Pointer to treat our struct as a raw array of bytes
  uint8_t* data_ptr = (uint8_t*)&telemetry;
  uint16_t payload_size = sizeof(TelemetryPacket); // 68 bytes

  // 4. Calculate XOR Checksum over the raw struct bytes
  uint8_t checksum = 0;
  for(int i = 0; i < payload_size; i++) {
    checksum ^= data_ptr[i];
  }

  // 5. Send Framed Packet
  Serial2.write(0xAA);                   // Start Marker
  Serial2.write(data_ptr, payload_size); // 68 Bytes of Telemetry
  Serial2.write(checksum);               // Checksum Byte
  Serial2.write(0x55);                   // End Marker

  Serial.print("Telemetry packet sent at ");
  Serial.print(telemetry.time_ms);
  Serial.println(" ms.");
  
  delay(500); // Every 0.5 seconds
}