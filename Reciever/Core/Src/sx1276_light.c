#include "sx1276_light.h"
#include "spi.h"

// Hardware Mapping for B-L072Z-LRWAN1 internal connections
#define LORA_NSS_PORT   GPIOA
#define LORA_NSS_PIN    GPIO_PIN_15
#define LORA_RESET_PORT GPIOC
#define LORA_RESET_PIN  GPIO_PIN_0

void SX1276_WriteRegister(uint8_t address, uint8_t value) {
    uint8_t buffer[2];
    buffer[0] = address | 0x80; // SPI write bit assignment
    buffer[1] = value;

    HAL_GPIO_WritePin(LORA_NSS_PORT, LORA_NSS_PIN, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, buffer, 2, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(LORA_NSS_PORT, LORA_NSS_PIN, GPIO_PIN_SET);
}

uint8_t SX1276_ReadRegister(uint8_t address) {
    uint8_t addr_byte = address & 0x7F; // SPI read bit assignment
    uint8_t val_byte = 0;

    HAL_GPIO_WritePin(LORA_NSS_PORT, LORA_NSS_PIN, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, &addr_byte, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi1, &val_byte, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(LORA_NSS_PORT, LORA_NSS_PIN, GPIO_PIN_SET);

    return val_byte;
}

void SX1276_Init(void) {
    // 1. Hard Reset the Radio
    HAL_GPIO_WritePin(LORA_RESET_PORT, LORA_RESET_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(LORA_RESET_PORT, LORA_RESET_PIN, GPIO_PIN_SET);
    HAL_Delay(10);

    // 2. Put into Sleep mode to enable LoRa mode change
    SX1276_WriteRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
    HAL_Delay(5);

    // 3. Jump to Standby Mode
    SX1276_WriteRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STANDBY);

    // 4. Configure Channel Frequency to 868.0 MHz
    // Formula: FRF = Freq_Hz / (32000000 / 2^19) -> 868000000 / 61.03515625 = 14221312 = 0xD90000
    SX1276_WriteRegister(REG_FRF_MSB, 0xD9);
    SX1276_WriteRegister(REG_FRF_MID, 0x00);
    SX1276_WriteRegister(REG_FRF_LSB, 0x00);

    // 5. Signal Configurations: Bandwidth 125 kHz, Coding Rate 4/5, Explicit Header Mode
    SX1276_WriteRegister(REG_MODEM_CONFIG_1, 0x72);

    // 6. Spreading Factor 7, TX Continuous off
    SX1276_WriteRegister(REG_MODEM_CONFIG_2, 0x70);

    // 7. Power Configuration: Max out PA_BOOST pin to +14dBm
    SX1276_WriteRegister(REG_PA_CONFIG, 0x8F);
}

void SX1276_SendPacket(uint8_t *payload, uint8_t size) {
    // Stage to Standby
    SX1276_WriteRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STANDBY);

    // Reset FIFO memory pointers to zero point
    SX1276_WriteRegister(REG_FIFO_ADDR_PTR, 0x00);
    SX1276_WriteRegister(REG_FIFO_TX_BASE_ADDR, 0x00);

    // Set dynamic load sizes
    SX1276_WriteRegister(REG_PAYLOAD_LENGTH, size);

    // Stream payload data byte sequence directly to radio internal FIFO
    for (uint8_t i = 0; i < size; i++) {
        SX1276_WriteRegister(REG_FIFO, payload[i]);
    }

    // Clear old state tracking interrupt triggers
    SX1276_WriteRegister(REG_IRQ_FLAGS, 0xFF);

    // Map DIO0 to Fire on TXDone status update
    SX1276_WriteRegister(REG_DIO_MAPPING_1, 0x40);

    // Ignite Transmitter execution line!
    SX1276_WriteRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);

    // Block until transmission completes by reading status register flags
    while ((SX1276_ReadRegister(REG_IRQ_FLAGS) & 0x08) == 0) {
        // Polling loop waiting for TxDone bit 3 to go high
    }

    // Clear transmission complete interrupt bit flag
    SX1276_WriteRegister(REG_IRQ_FLAGS, 0x08);

    // Park the transceiver safely back in low-current Standby Mode
    SX1276_WriteRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STANDBY);
}
// Append these functions to your existing sx1276_light.c file

void SX1276_StartListening(void) {
    // Stage to Standby first to clear old states safely
    SX1276_WriteRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STANDBY);

    // Point internal radio pointer back to the base RX memory region
    SX1276_WriteRegister(REG_FIFO_ADDR_PTR, 0x00);
    SX1276_WriteRegister(REG_PAYLOAD_LENGTH, 68);

    // Clear any leftover flag indicators
    SX1276_WriteRegister(REG_IRQ_FLAGS, 0xFF);

    // Put the transceiver into Continuous RX Mode
    // It will now constantly scan the airwaves for preambles
    SX1276_WriteRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | 0x05); // 0x05 is RXCONTINUOUS
}

uint8_t SX1276_CheckForPacket(uint8_t *rx_buffer, uint8_t max_size) {
    uint8_t irq_flags = SX1276_ReadRegister(REG_IRQ_FLAGS);

    if (irq_flags & 0x40) { // RxDone
        SX1276_WriteRegister(REG_IRQ_FLAGS, 0xFF); // Clear flags

        // --- CRUCIAL CHANGE HERE ---
        // Read register 0x13 (RegRxNbBytes) to find the actual over-the-air packet size
        uint8_t received_bytes = SX1276_ReadRegister(0x13);

        if (received_bytes > max_size) {
            received_bytes = max_size;
        }

        // Set FIFO pointer to the start of this packet
        uint8_t rx_current_addr = SX1276_ReadRegister(0x10);
        SX1276_WriteRegister(REG_FIFO_ADDR_PTR, rx_current_addr);

        // Read data out
        for (uint8_t i = 0; i < received_bytes; i++) {
            rx_buffer[i] = SX1276_ReadRegister(REG_FIFO);
        }

        SX1276_StartListening(); // Return to listening mode
        return received_bytes;
    }
    return 0;
}
