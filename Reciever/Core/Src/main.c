/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "rtc.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include "sx1276_light.h"//edit
/* USER CODE END Includes */
/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */
/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */
/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */
/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
uint8_t lora_rx_buffer[128] = {0};// we update this with every data byte we recieve to check against esp32's math later.
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
struct TelemetryPacket telemetry_data;// The states for our UART packet reader

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */
/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {// this is a hardware interrupt. It is a reflex. Every single time the stm3 recieved t byte into rxbyte , it pauses the main loop , jumps here and runs the code instantly and goes back
//   if (huart->Instance == USART1) { // this checks makes sure that the interrupt came from usart1 and not any other serial port.
//       switch (rx_state) {
//           case STATE_WAIT_START:
//               if (rx_byte == 0xAA) {
//                   rx_index = 0;
//                   rx_checksum = 0;
//                   rx_state = STATE_READ_PAYLOAD;
//               }
//               break;
//           case STATE_READ_PAYLOAD:
//               rx_buffer[rx_index++] = rx_byte;
//               rx_checksum ^= rx_byte; // Update running checksum
//               if (rx_index >= 68) {
//                   rx_state = STATE_READ_CHECKSUM;
//               }
//               break;
//           case STATE_READ_CHECKSUM:
//               if (rx_byte == rx_checksum) {
//                   rx_state = STATE_WAIT_END;
//               } else {
//                   rx_state = STATE_WAIT_START; // Corruption detected, drop packet
//               }
//               break;
//           case STATE_WAIT_END:
//               if (rx_byte == 0x55) {
//                   packet_ready = 1; // Success! We have a perfect 64-byte packet
//               }
//               rx_state = STATE_WAIT_START; // Reset for the next packet
//               break;
//       }
//       // Instantly re-arm the interrupt to catch the very next byte
//       HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
//   }
//}
///* USER CODE END 0 */
/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
 /* USER CODE BEGIN 1 */
 /* USER CODE END 1 */
 /* MCU Configuration--------------------------------------------------------*/
 /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
 HAL_Init();
 /* USER CODE BEGIN Init */
 /* USER CODE END Init */
 /* Configure the system clock */
 SystemClock_Config();
 /* USER CODE BEGIN SysInit */
 /* USER CODE END SysInit */
 /* Initialize all configured peripherals */
 MX_GPIO_Init();
 MX_RTC_Init();
 MX_SPI1_Init();
 MX_USART1_UART_Init();
 MX_USART2_UART_Init();
 /* USER CODE BEGIN 2 */
 char boot_msg[] = "\r\n--- STM32 HAL LoRa Receiver Ready. Scanning airwaves... ---\r\n";
   HAL_UART_Transmit(&huart2, (uint8_t *)boot_msg, sizeof(boot_msg) - 1, HAL_MAX_DELAY);

   // 1. Force the Board's Internal RF Antenna Switch GPIOs to be Outputs
   GPIO_InitTypeDef GPIO_InitStruct = {0};
   __HAL_RCC_GPIOA_CLK_ENABLE();
   __HAL_RCC_GPIOC_CLK_ENABLE();

   GPIO_InitStruct.Pin = GPIO_PIN_1;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

   GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2;
   HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

   // 2. Set the Switches to High-Frequency RX Mode (868 / 915 MHz path)
   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);   // RFI_HF = ON
   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET); // RFO_LF = OFF
   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_RESET); // PA_BOOST = OFF

   // 3. Initialize and Start the Radio Peripheral
   SX1276_Init();
   SX1276_StartListening(); /* USER CODE END 2 */
 /* Infinite loop */
 /* USER CODE BEGIN WHILE */
 while (1)
 {
      // Keep checking if the radio found an arriving packet
      uint8_t packet_size = SX1276_CheckForPacket(lora_rx_buffer, 68);

      if (packet_size > 0) {
          // Verify it matches the 68-byteTelemetryPacket size
    	  if (packet_size == sizeof(telemetry_data)) {

    	               // Directly re-map raw air byte buffers into our cleanly cast structural memory
    	               memcpy(&telemetry_data, lora_rx_buffer, sizeof(telemetry_data));

    	               // INCREASE LOG BUFFER TO 256 TO PREVENT OVERFLOW
    	               char log_msg[256];
    	               snprintf(log_msg, sizeof(log_msg),
    	                        "[LORA RX] Time: %lu ms | RPM: %lu | Motor: %.1f C | MCU: %.1f C | AccelX: %.2f\r\n",
    	                        telemetry_data.time_ms,
    	                        telemetry_data.rpm,
    	                        telemetry_data.motor_temp,
    	                        telemetry_data.mcu_temp,
    	                        telemetry_data.ax);

    	               HAL_UART_Transmit(&huart2, (uint8_t *)log_msg, strlen(log_msg), HAL_MAX_DELAY);
    	           } else {
              // This will print the exact number of corrupted bytes landing in your FIFO
              char err_msg[64];
              snprintf(err_msg, sizeof(err_msg), "[WARNING] Expected 68, but got length: %d\r\n", packet_size);
              HAL_UART_Transmit(&huart2, (uint8_t *)err_msg, strlen(err_msg), HAL_MAX_DELAY);
          }
      }

      // Quick delay buffer to prevent pinning the internal CPU cycles unnecessarily hard
      HAL_Delay(5);
    /* USER CODE END WHILE */
  }
 /* USER CODE END 3 */
}
/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
 RCC_OscInitTypeDef RCC_OscInitStruct = {0};
 RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
 RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
 /** Configure the main internal regulator output voltage
 */
 __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
 /** Initializes the RCC Oscillators according to the specified parameters
 * in the RCC_OscInitTypeDef structure.
 */
 RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
 RCC_OscInitStruct.HSIState = RCC_HSI_ON;
 RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
 RCC_OscInitStruct.LSIState = RCC_LSI_ON;
 RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
 RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
 RCC_OscInitStruct.PLL.PLLMUL = RCC_PLLMUL_6;
 RCC_OscInitStruct.PLL.PLLDIV = RCC_PLLDIV_3;
 if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
 {
   Error_Handler();
 }
 /** Initializes the CPU, AHB and APB buses clocks
 */
 RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                             |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
 RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
 RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
 RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
 RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
 if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
 {
   Error_Handler();
 }
 PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_USART2
                             |RCC_PERIPHCLK_RTC;
 PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
 PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
 PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
 if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
 {
   Error_Handler();
 }
}
/* USER CODE BEGIN 4 */
/* USER CODE END 4 */
/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
 /* USER CODE BEGIN Error_Handler_Debug */
 /* User can add his own implementation to report the HAL error return state */
 __disable_irq();
 while (1)
 {
 }
 /* USER CODE END Error_Handler_Debug */
}
#ifdef  USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 * where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
 /* USER CODE BEGIN 6 */
 /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
 /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

