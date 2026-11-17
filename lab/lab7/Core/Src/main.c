/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.c
  * @brief   Lab 7 DAC signal generator (NUCLEO-G474RE)
  *          TIM6 interrupt every 0.2 ms -> DAC1 (PA4) outputs one point
  *          100 points per period -> 5 kHz / 100 = 50 Hz
  *          USER button (B1 / PC13): sine -> triangle -> square -> sine
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <math.h>
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
DAC_HandleTypeDef hdac1;
TIM_HandleTypeDef htim6;

/* USER CODE BEGIN PV */
uint16_t sine_table[100];               // sine wave table
uint16_t tri_table[100];                // triangle wave table
uint16_t sq_table[100];                 // square wave table

volatile uint16_t *wave = sine_table;   // table being output now (sine at start)
volatile uint8_t  idx  = 0;             // index of the next point
volatile uint8_t  mode = 0;             // 0=sine 1=triangle 2=square
volatile uint32_t last_press = 0;       // for button debounce
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DAC1_Init(void);
static void MX_TIM6_Init(void);

/**
  * @brief  The application entry point.
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_DAC1_Init();
  MX_TIM6_Init();

  /* USER CODE BEGIN 2 */
  /* Build 3 tables, range 248 ~ 3848 (center 2048 = about 1.65 V) */
  for (int i = 0; i < 100; i++) {
    /* Sine: angle of point i = 2*pi*i/100 */
    sine_table[i] = (uint16_t)(2048 + 1800 * sinf(2 * 3.14159f * i / 100));

    /* Triangle: first 50 points go up, last 50 go down */
    if (i < 50) tri_table[i] = (uint16_t)(248 + 3600 * i / 50);
    else        tri_table[i] = (uint16_t)(3848 - 3600 * (i - 50) / 50);

    /* Square: first half high, second half low */
    sq_table[i] = (i < 50) ? 3848 : 248;
  }

  HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);   // start DAC1 channel 1 (PA4)
  HAL_TIM_Base_Start_IT(&htim6);          // start TIM6 with interrupt
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE BEGIN 3 */
    /* Nothing here: the waveform is made by the TIM6 interrupt */
    /* USER CODE END 3 */
  }
}

/**
  * @brief System Clock Configuration: HSI 16 MHz -> PLL -> 170 MHz
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief DAC1 init: OUT1 -> PA4, buffer on, trigger none
  */
static void MX_DAC1_Init(void)
{
  DAC_ChannelConfTypeDef sConfig = {0};

  hdac1.Instance = DAC1;
  if (HAL_DAC_Init(&hdac1) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.DAC_HighFrequency = DAC_HIGH_FREQUENCY_INTERFACE_MODE_AUTOMATIC;
  sConfig.DAC_DMADoubleDataMode = DISABLE;
  sConfig.DAC_SignedFormat = DISABLE;
  sConfig.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
  sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
  sConfig.DAC_Trigger2 = DAC_TRIGGER_NONE;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_EXTERNAL;
  sConfig.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
  if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM6 init: 170 MHz / 170 = 1 MHz; 1 MHz / 200 = 5 kHz (every 0.2 ms)
  */
static void MX_TIM6_Init(void)
{
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 169;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 199;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO init: PC13 (USER button) as external interrupt
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* USER CODE BEGIN 4 */
/* Called by TIM6 every 0.2 ms: send the next point to the DAC */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6) {
    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, wave[idx]);
    idx++;
    if (idx >= 100) idx = 0;
  }
}

/* USER button pressed: switch to the next waveform */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_13) {
    uint32_t now = HAL_GetTick();
    if (now - last_press < 200) return;   // ignore bounces within 200 ms
    last_press = now;

    mode = (mode + 1) % 3;
    if (mode == 0)      wave = sine_table;
    else if (mode == 1) wave = tri_table;
    else                wave = sq_table;
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
