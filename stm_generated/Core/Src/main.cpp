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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <STM_can_to_pwm.h>
#include <stm_can_interface.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// Memory addresses for variables stored in flash memory
#define FLASH_USER_MODE 0x0801E800
#define FLASH_USER_ID 0x0801F000
#define FLASH_USER_UPDATE_RATE 0x0801F800
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

FDCAN_HandleTypeDef hfdcan1;

I2C_HandleTypeDef hi2c2;

/* USER CODE BEGIN PV */
uint8_t mode_page = 61;
uint8_t id_page = 62;
uint8_t update_rate_page = 63;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_FDCAN1_Init(void);
static void MX_I2C2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Writes data to flash memory
static void Flash_Write(uint8_t data, uint8_t page)
{
  HAL_FLASH_Unlock();

  // Clear any error flags
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPERR | FLASH_FLAG_PROGERR | FLASH_FLAG_WRPERR);

  FLASH_EraseInitTypeDef eraseInitStruct;
  uint32_t pageError;

  eraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
  eraseInitStruct.Banks = FLASH_BANK_1;
  eraseInitStruct.Page = page;
  eraseInitStruct.NbPages = 1;

  // Erase the data at the address
  HAL_FLASHEx_Erase(&eraseInitStruct, &pageError);

  uint64_t data64 = (uint64_t)data;

  // Write data to flash that represents the mode
  if (page == 61)
  {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, FLASH_USER_MODE, data64);
  }
  // Write data to flash that represents the ID
  else if (page == 62)
  {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, FLASH_USER_ID, data64);
  }
  // Write data to flash that represents the update rate
  else if (page == 63)
  {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, FLASH_USER_UPDATE_RATE, data64);
  }
  HAL_FLASH_Lock();
}

// Reads data from flash memory
static uint8_t Flash_Read(uint8_t page)
{
  uint64_t raw;
  // Read the mode from flash
  if (page == 61)
  {
    raw = *(uint64_t *)FLASH_USER_MODE;
  }
  // Read the ID from flash
  else if (page == 62)
  {
    raw = *(uint64_t *)FLASH_USER_ID;
  }
  // Read the update rate from flash
  else if (page == 63)
  {
    raw = *(uint64_t *)FLASH_USER_UPDATE_RATE;
  }

  if (raw == 0xFFFFFFFFFFFFFFFF)
  { // erased flash
    return -1;
  }
  return (uint8_t)raw;
}
/* USER CODE END 0 */

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
  MX_ADC1_Init();
  // MX_FDCAN1_Init();
  // MX_I2C2_Init();
  /* USER CODE BEGIN 2 */

  // FDCAN_TxHeaderTypeDef tx_header;
  // FDCAN_RxHeaderTypeDef rx_header;

  uint8_t rx_data[8] = {0};

  uint8_t can_id = Flash_Read(id_page);
  uint8_t can_hardware_id = 0x00;

  // Read all the hardware identifier pins
  GPIO_PinState gpio_state_1 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8);
  GPIO_PinState gpio_state_2 = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2);
  GPIO_PinState gpio_state_3 = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1);
  GPIO_PinState gpio_state_4 = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0);

  if (gpio_state_1 == GPIO_PIN_SET)
  {
    can_hardware_id = can_hardware_id | 0x01;
  }
  if (gpio_state_2 == GPIO_PIN_SET)
  {
    can_hardware_id = can_hardware_id | 0x02;
  }
  if (gpio_state_3 == GPIO_PIN_SET)
  {
    can_hardware_id = can_hardware_id | 0x04;
  }
  if (gpio_state_4 == GPIO_PIN_SET)
  {
    can_hardware_id = can_hardware_id | 0x08;
  }

  if (can_id == 255)
  {
    can_id = can_hardware_id;
  }

  // Setup the header for FDCAN communication
  // tx_header.Identifier = (can_hardware_id << 16);
  // tx_header.IdType = FDCAN_EXTENDED_ID;
  // tx_header.TxFrameType = FDCAN_DATA_FRAME;
  // tx_header.DataLength = FDCAN_DLC_BYTES_4;
  // tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  // tx_header.BitRateSwitch = FDCAN_BRS_OFF;
  // tx_header.FDFormat = FDCAN_CLASSIC_CAN;
  // tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  // tx_header.MessageMarker = 0x04;

  HAL_StatusTypeDef state;

  state = HAL_FDCAN_Start(&hfdcan1);

  uint16_t pwm_id = 0x43;

  uint8_t sleep = 0x20 | 0x10;
  state = HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, 0x00, I2C_MEMADD_SIZE_8BIT, &sleep, 1, 1000);

  uint8_t mode2 = 0x04; // totem pole
  state = HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, 0x01, I2C_MEMADD_SIZE_8BIT, &mode2, 1, 1000);

  uint8_t prescale = 0x79;
  state = HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, 0xFE, I2C_MEMADD_SIZE_8BIT, &prescale, 1, 1000);

  uint8_t wake = 0x20;
  state = HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, 0x00, I2C_MEMADD_SIZE_8BIT, &wake, 1, 1000);

  int cnt = 1;
  uint16_t off = 205;

  // Set the mode and update rate from data stored in flash memory
  uint8_t mode_select = Flash_Read(mode_page);
  uint64_t update_rate = Flash_Read(update_rate_page);

  // uint16_t temp1 = (can_id << 8) | 0x10;
  // uint16_t temp2 = (can_id << 8) | 0x20;
  // uint16_t temp3 = (can_id << 8) | 0x30;
  // uint16_t temp4 = (can_id << 8) | 0x40;
  // uint16_t temp5 = (can_id << 8) | 0x50;
  // uint16_t temp6 = (can_id << 8) | 0x60;
  // uint16_t temp7 = (can_id << 8) | 0x70;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  //// TODO: we need to indicate if servo here and the node id

  // STM_can_to_pwm<Stm_can_iface, STM32_system_clock> can_to_pwm{0, true, 10};
  // can_to_pwm.initialize();

  volatile uint32_t counter{0};
  while (counter++ < 1'000'000)
    ;

  // can_to_pwm.main();

  while (1)
  {
    // HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &rx_header, rx_data);

    //// If header matches sensor and object id is 1, then set the mode
    // if (rx_header.Identifier == temp1 && (rx_data[0] == 1 || rx_data[0] == 2))
    //{
    //   if (mode_select != rx_data) // avoid unnecessary flash writes
    //   {
    //     mode_select = rx_data[0];
    //     Flash_Write(mode_select, mode_page);
    //   }
    // }

    //// If header matches sensor and object id is 2, then set device id
    // else if (rx_header.Identifier == temp2)
    //{
    //   if (can_id != rx_data[0]) // avoid unnecessary flash writes
    //   {
    //     can_id = rx_data[0];
    //     tx_header.Identifier = (can_id << 16);
    //     Flash_Write(can_id, id_page);
    //   }
    // }

    //// If header matches sensor and object id is 3, then set the update rate
    // else if (rx_header.Identifier == temp3)
    //{
    //   if (update_rate != rx_data[0]) // avoid unnecessary flash writes
    //   {
    //     update_rate = rx_data[0];
    //     Flash_Write(update_rate, update_rate_page);
    //   }
    // }

    //// If header matches sensor and object id is 4, then reset any configurations
    // else if (rx_header.Identifier == temp4)
    //{
    //   mode_select = 0x01;
    //   can_id = can_hardware_id;
    //   update_rate = 0x05;

    //  Flash_Write(mode_select, mode_page);
    //  Flash_Write(can_id, id_page);
    //  Flash_Write(update_rate, update_rate_page);
    //}

    // else if ((rx_header.Identifier & 0xF0) == 0x50)
    //{
    //   uint16_t off = (rx_data[0] << 8) | rx_data[1];
    //   uint8_t data0[4] = {
    //       0x00,             // ON_L
    //       0x00,             // ON_H
    //       off & 0xFF,       // OFF_L
    //       (off >> 8) & 0x0F // OFF_H
    //   };
    //   uint8_t addr = 0;
    //   int valid = 1;
    //   switch (rx_header.Identifier & 0xFF)
    //   {
    //   case 0x50:
    //     addr = 0xFA;
    //     break;
    //   case 0x51:
    //     addr = 0x06;
    //     break;
    //   case 0x52:
    //     addr = 0x0A;
    //     break;
    //   case 0x53:
    //     addr = 0x0E;
    //     break;
    //   case 0x54:
    //     addr = 0x12;
    //     break;
    //   default:
    //     valid = 0;
    //     break;
    //   }
    //   if (valid == 1)
    //   {
    //     HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, addr, I2C_MEMADD_SIZE_8BIT, data0, 4, 1000);
    //   }
    // }

    // else if ((rx_header.Identifier & 0xF0) == 0x60)
    //{
    //   uint16_t off = 0;
    //   uint8_t data1[4] = {
    //       0x00, // ON_L
    //       0x00, // ON_H
    //       0x33, // OFF_L
    //       0x01  // OFF_H
    //   };
    //   uint8_t addr = 0;
    //   int valid = 1;
    //   switch (rx_header.Identifier & 0xFF)
    //   {
    //   case 0x60:
    //     addr = 0xFA;
    //     break;
    //   case 0x61:
    //     addr = 0x06;
    //     break;
    //   case 0x62:
    //     addr = 0x0A;
    //     break;
    //   case 0x63:
    //     addr = 0x0E;
    //     break;
    //   case 0x64:
    //     addr = 0x12;
    //     break;
    //   default:
    //     valid = 0;
    //     break;
    //   }
    //   if (valid == 1)
    //   {
    //     HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, addr, I2C_MEMADD_SIZE_8BIT, data1, 4, 1000);
    //   }
    // }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
   */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief ADC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
   */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.LowPowerAutoPowerOff = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_1CYCLE_5;
  hadc1.Init.SamplingTimeCommon2 = ADC_SAMPLETIME_1CYCLE_5;
  hadc1.Init.OversamplingMode = DISABLE;
  hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
   */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */
}

/**
 * @brief FDCAN1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_FDCAN1_Init(void)
{

  /* USER CODE BEGIN FDCAN1_Init 0 */

  /* USER CODE END FDCAN1_Init 0 */

  /* USER CODE BEGIN FDCAN1_Init 1 */

  /* USER CODE END FDCAN1_Init 1 */
  hfdcan1.Instance = FDCAN1;
  hfdcan1.Init.ClockDivider = FDCAN_CLOCK_DIV1;
  hfdcan1.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan1.Init.AutoRetransmission = DISABLE;
  hfdcan1.Init.TransmitPause = DISABLE;
  hfdcan1.Init.ProtocolException = DISABLE;
  hfdcan1.Init.NominalPrescaler = 4;
  hfdcan1.Init.NominalSyncJumpWidth = 1;
  hfdcan1.Init.NominalTimeSeg1 = 2;
  hfdcan1.Init.NominalTimeSeg2 = 1;
  hfdcan1.Init.DataPrescaler = 1;
  hfdcan1.Init.DataSyncJumpWidth = 1;
  hfdcan1.Init.DataTimeSeg1 = 1;
  hfdcan1.Init.DataTimeSeg2 = 1;
  hfdcan1.Init.StdFiltersNbr = 0;
  hfdcan1.Init.ExtFiltersNbr = 0;
  hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN FDCAN1_Init 2 */

  /* USER CODE END FDCAN1_Init 2 */
}

/**
 * @brief I2C2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x00503D58;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
   */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
   */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pins : PB0 PB1 PB2 */
  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PA8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
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
