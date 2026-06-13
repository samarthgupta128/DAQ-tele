/**
  ******************************************************************************
  * @file    subghz_phy_app.c
  * @author  MCD Application Team / Custom DAQ Node
  * @brief   Application of the SubGHz_Phy Middleware (Car Transmitter - SPI2 SLAVE)
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "platform.h"
#include "sys_app.h"
#include "subghz_phy_app.h"
#include "radio.h"
#include "app_version.h"

/* USER CODE BEGIN Includes */
#include "stm32_timer.h"
#include "stm32_seq.h"
#include "utilities_def.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

// --- NEW TELEMETRY DATA STRUCTURE (49 Bytes) ---
// This exactly mirrors the ESP32 C++ Struct
#pragma pack(1)
typedef struct {
    uint8_t  sync1;       // 0xAA
    uint8_t  sync2;       // 0xBB
    uint16_t packet_id;
    uint16_t rpm;
    float    mcu_temp;
    float    motor_temp;
    float    ax;
    float    ay;
    float    az;
    float    yaw;
    float    pitch;
    float    roll;
    uint16_t pot1;
    uint16_t pot2;
    uint16_t pot3;
    uint16_t pot4;
    uint16_t pot5;
    uint8_t  checksum;
} TelemetryPacket;
#pragma pack()

typedef enum
{
  RX,
  RX_TIMEOUT,
  RX_ERROR,
  TX,
  TX_TIMEOUT,
} States_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
#define RX_TIMEOUT_VALUE              3000
#define TX_TIMEOUT_VALUE              3000
#define MAX_APP_BUFFER_SIZE           255
#define LED_PERIOD_MS                 200

/* Private variables ---------------------------------------------------------*/
static RadioEvents_t RadioEvents;

/* USER CODE BEGIN PV */
// --- SPI2 & ESP32 VARIABLES ---
SPI_HandleTypeDef hspi2;  // Changed to hspi2 to avoid naming collision
TelemetryPacket spiData;
volatile bool newSpiDataReady = false;

// --- STANDARD VARIABLES ---
static States_t State = RX;
static uint8_t BufferRx[MAX_APP_BUFFER_SIZE];
static uint8_t BufferTx[MAX_APP_BUFFER_SIZE];
uint16_t RxBufferSize = 0;
int8_t RssiValue = 0;
int8_t SnrValue = 0;
static UTIL_TIMER_Object_t timerLed;
static int32_t random_delay;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void OnTxDone(void);
static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t LoraSnr_FskCfo);
static void OnTxTimeout(void);
static void OnRxTimeout(void);
static void OnRxError(void);
static void OnledEvent(void *context);
static void PingPong_Process(void);

/* USER CODE BEGIN PFP */
// --- MANUAL SPI2 SLAVE INITIALIZATION ---
static void MX_SPI2_Init(void)
{
  __HAL_RCC_SPI2_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE(); // SPI2 uses Port B

  // Configure PB12(NSS), PB13(SCK), PB14(MISO), PB15(MOSI)
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF0_SPI2;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // Configure SPI2 in SLAVE mode
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_SLAVE;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES_RXONLY;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_HARD_INPUT; // Hardware controls the CS pin
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;

  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    while(1) {}
  }

  // Enable Interrupts for SPI2
  HAL_NVIC_SetPriority(SPI2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(SPI2_IRQn);
}

// --- CHECKSUM CALCULATOR ---
// Validates the integrity of the incoming payload
uint8_t CalculateChecksum(TelemetryPacket *packet) {
    uint8_t *bytePtr = (uint8_t *)packet;
    uint8_t calculatedXOR = 0;
    // XOR all bytes except the very last one (which is the checksum itself)
    for (size_t i = 0; i < sizeof(TelemetryPacket) - 1; i++) {
        calculatedXOR ^= bytePtr[i];
    }
    return calculatedXOR;
}
/* USER CODE END PFP */

/* Exported functions ---------------------------------------------------------*/
void SubghzApp_Init(void)
{
  APP_LOG(TS_OFF, VLEVEL_M, "\n\rTELEMETRY DAQ NODE (SPI2 SLAVE)\n\r");

  UTIL_TIMER_Create(&timerLed, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, OnledEvent, NULL);
  UTIL_TIMER_SetPeriod(&timerLed, LED_PERIOD_MS);
  UTIL_TIMER_Start(&timerLed);

  RadioEvents.TxDone = OnTxDone;
  RadioEvents.RxDone = OnRxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxTimeout = OnRxTimeout;
  RadioEvents.RxError = OnRxError;

  Radio.Init(&RadioEvents);

  random_delay = (Radio.Random()) >> 22;

  /* Radio Set frequency - HARDCODED FOR ISOLATION */
  Radio.SetChannel(868100000);

#if ((USE_MODEM_LORA == 1) && (USE_MODEM_FSK == 0))
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, TX_TIMEOUT_VALUE);

  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);

  Radio.SetMaxPayloadLength(MODEM_LORA, MAX_APP_BUFFER_SIZE);
#endif

  LED_Init(LED_RED1);
  LED_Init(LED_RED2);
  memset(BufferTx, 0x0, MAX_APP_BUFFER_SIZE);

  Radio.Rx(RX_TIMEOUT_VALUE + random_delay);

  /* USER CODE BEGIN SubghzApp_Init_2 */
  // Initialize our manual SPI2
  MX_SPI2_Init();

  // Start listening to the ESP32 in the background continuously on SPI2
  HAL_SPI_Receive_IT(&hspi2, (uint8_t *)&spiData, sizeof(TelemetryPacket));
  /* USER CODE END SubghzApp_Init_2 */

  UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process), UTIL_SEQ_RFU, PingPong_Process);
}

/* Private functions ---------------------------------------------------------*/
static void OnTxDone(void)
{
  State = TX;
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process), CFG_SEQ_Prio_0);
}

static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t LoraSnr_FskCfo)
{
  State = RX;
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process), CFG_SEQ_Prio_0);
}

static void OnTxTimeout(void)
{
  State = TX_TIMEOUT;
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process), CFG_SEQ_Prio_0);
}

static void OnRxTimeout(void)
{
  State = RX_TIMEOUT;
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process), CFG_SEQ_Prio_0);
}

static void OnRxError(void)
{
  State = RX_ERROR;
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process), CFG_SEQ_Prio_0);
}

static void OnledEvent(void *context)
{
  LED_Toggle(LED_RED1);
  LED_Toggle(LED_RED2);
  UTIL_TIMER_Start(&timerLed);
}

/* USER CODE BEGIN PrFD */

// Triggered automatically when 49 bytes finish arriving from the ESP32 via SPI2
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi->Instance == SPI2)
  {
    newSpiDataReady = true;

    // Immediately prime the SPI to catch the next 49 bytes
    HAL_SPI_Receive_IT(&hspi2, (uint8_t *)&spiData, sizeof(TelemetryPacket));
  }
}

// Main Process Loop
static void PingPong_Process(void)
{
  Radio.Sleep();

  // If the ESP32 has sent us fresh data via SPI...
  if (newSpiDataReady == true)
  {
    newSpiDataReady = false;

    // 1. Verify Sync Headers to ensure we didn't miss a clock cycle
    if (spiData.sync1 == 0xAA && spiData.sync2 == 0xBB)
    {
      // 2. Verify Checksum to ensure no data was corrupted by motor noise
      if (spiData.checksum == CalculateChecksum(&spiData))
      {
        // Validation passed! Blast the 49-byte packet over LoRa!
        Radio.Send((uint8_t *)&spiData, sizeof(TelemetryPacket));
      }
      else
      {
        APP_LOG(TS_OFF, VLEVEL_M, "SPI Error: Checksum mismatch!\r\n");
      }
    }
    else
    {
      APP_LOG(TS_OFF, VLEVEL_M, "SPI Error: Sync bytes missing!\r\n");
    }
  }
  else
  {
    // Wait slightly and check again
    HAL_Delay(10);
    UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process), CFG_SEQ_Prio_0);
  }
}

/* USER CODE END PrFD */
