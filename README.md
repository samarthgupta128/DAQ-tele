This repo contains the stm32 files for DAC.
The data is being sent over from the LORA module in car to another recieving lora module.


# Command to read the incoming data

sudo minicom -D /dev/ttyACM0 -b 115200

- The name of device may be different.
- Need to install minicom.

