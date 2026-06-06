This repo contains the stm32 files for DAC.
The data is being sent over from the LORA module in car to another recieving lora module.


# Command to read the incoming data

sudo minicom -D /dev/ttyACM0 -b 115200

- The name of device may be different.
- Need to install minicom.

# 36 byte C struct 
We upgraded the architecture to use a packed Binary C-Struct. By turning decimals into whole numbers (e.g., sending 20.5C as 205), we compress all 17 sensor readings into a tiny, highly efficient 36-byte packet.

How it works end-to-end:

-  The Car (Transmitter): The STM32 packs the sensor data into a 36-byte block of memory and broadcasts it over LoRa.

- The Base Station (Receiver): The laptop's STM32 catches the packet and immediately translates the raw binary into a clean Hexadecimal string over USB (e.g., 672000000700CD00...).

- The Python GUI: The dashboard reads the hex string, decodes it back into numbers using Python's struct library, and instantly updates the live gauges and CSV  logs.