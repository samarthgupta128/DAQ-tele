#ifndef INC_SX1276_LIGHT_H_
#define INC_SX1276_LIGHT_H_

#include "main.h"

// SX1276 Register Addresses
#define REG_FIFO                    0x00
#define REG_OP_MODE                 0x01
#define REG_FRF_MSB                 0x06
#define REG_FRF_MID                 0x07
#define REG_FRF_LSB                 0x08
#define REG_PA_CONFIG               0x09
#define REG_FIFO_ADDR_PTR           0x0D
#define REG_FIFO_TX_BASE_ADDR       0x0E
#define REG_IRQ_FLAGS               0x12
#define REG_MODEM_CONFIG_1          0x1D
#define REG_MODEM_CONFIG_2          0x1E
#define REG_PAYLOAD_LENGTH          0x22
#define REG_DIO_MAPPING_1           0x40

// SX1276 Modes
#define MODE_LONG_RANGE_MODE        0x80
#define MODE_SLEEP                  0x00
#define MODE_STANDBY                0x01
#define MODE_TX                     0x03

// Function Prototypes
void SX1276_Init(void);
void SX1276_WriteRegister(uint8_t address, uint8_t value);
uint8_t SX1276_ReadRegister(uint8_t address);
void SX1276_SendPacket(uint8_t *payload, uint8_t size);
// Add these to the bottom of your existing sx1276_light.h file
void SX1276_StartListening(void);
uint8_t SX1276_CheckForPacket(uint8_t *rx_buffer, uint8_t max_size);
#endif /* INC_SX1276_LIGHT_H_ */
