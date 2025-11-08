/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <string.h>
#include "./Sources/SCHsensor.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

SCHResult DataSCH16;

static volatile bool SCH1_error_available = false;
SCHRawData SCH1_summed_data_buffer;

// Function prototypes
static void SystemClock_Config(void);
static void readingSCHData_callback(void);

char serialNum[15];

typedef struct{
	uint8_t timerCallback : 1;
	uint8_t uartCallback : 1;
}flag_t;

flag_t systemFlag = {0};
SCHResult Data;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	SCHFilter         Filter;
	SCHSensitivity    Sensitivity;
	SCHDecimation     Decimation;
	int  init_status;
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
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
	Filter.rate = FILTER_RATE;
	Filter.acc  = FILTER_ACC12;
	Filter.acc3 = FILTER_ACC3;

	// SCH1600 sensitivity settings
	Sensitivity.rate1 = SENSITIVITY_RATE1;
	Sensitivity.rate2 = SENSITIVITY_RATE2;
	Sensitivity.acc1  = SENSITIVITY_ACC1;
	Sensitivity.acc2  = SENSITIVITY_ACC2;
	Sensitivity.acc3  = SENSITIVITY_ACC3;

	// SCH1600 decimation settings (for Rate2 and Acc2 channels).
	Decimation.rate2 = DECIMATION_RATE;
	Decimation.acc2  = DECIMATION_ACC;
	// Initialize the sensor
	init_status = SCHInit(Filter, Sensitivity, Decimation, false);
	if (init_status != SCH_OK) {
		 HAL_Delay(50);
		 SCHReset();
		 NVIC_SystemReset();
	}
	/** read serial number sensor**/
	strcpy(serialNum, SCHGetSnbr());
	// With 1000 Hz sample rate and 10x averaging we get Output Data Rate (ODR) of 100 Hz.
	HAL_TIM_Base_Start_IT(&htim2);
	HAL_UART_Transmit_DMA(&huart1, (uint8_t*)&Data, sizeof(Data));


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
  	/***If  SCH sensor has a problem, it will be restarted. ***/
		if (SCH1_error_available) {
				// Error from sensor. Stop sample timer to avoid race condition when reading status.
				HAL_TIM_Base_Stop_IT(&htim2);
				SCH1_error_available = false;
				// Read SCH1600 status registers
				SCHStatus Status;
				SCHGetStatus(&Status);
				// Restart sampling timer
				HAL_TIM_Base_Start_IT(&htim2);
		}
		/*** reading SCH sensor data every 1ms  ***/
		if(systemFlag.timerCallback && systemFlag.uartCallback)
		{
			systemFlag.timerCallback = 0;
			systemFlag.uartCallback = 0;
			readingSCHData_callback();
			HAL_UART_Transmit_DMA(&huart1, (uint8_t*)&Data, sizeof(Data));
		}
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxHalfCallback(UART_HandleTypeDef* huart)
{
		switch ((uint32_t) huart->Instance)
	  {
	    case USART1_BASE:

	      break;
	    default:

	      break;
	  }
}
void HAL_UART_TxHalfCallback(UART_HandleTypeDef* huart)
{
		switch ((uint32_t) huart->Instance)
	  {
	    case USART1_BASE:

	      break;
	    default:

	      break;
	  }
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart) {
		switch ((uint32_t) huart->Instance)
	  {
				case USART1_BASE:

					break;
				default:

					break;
	  }

}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart)
{
  switch ((uint32_t) huart->Instance)
  {
			case USART1_BASE:
				systemFlag.uartCallback = 1;
				break;
			default:

				break;
  }
}
/*** reading SCH sensor data  ***/
static void readingSCHData_callback(void)
{
    SCHGetData(&SCH1_summed_data_buffer);
    SCHGetData2(&SCH1_summed_data_buffer);
    if (SCH1_summed_data_buffer.frameError)
        SCH1_error_available = true;

    SCHConvertData(&SCH1_summed_data_buffer, &Data);
}

/*** TIMER 2  1000HZ Or 1ms ***/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &htim2)
    {
    	systemFlag.timerCallback = 1;
    }
}





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
