/* USER CODE BEGIN Header */
/**
  * Lab6 : ADC x DMA 聲控律動燈
  * - TIM3 每 125 us 觸發 ADC1 (8000 Hz)
  * - DMA Circular + Ping-pong (256 筆, 兩半各 128 筆)
  * - PC6 紅燈：拍手/彈指 切換（基本題）
  * - PA5 / D13 綠燈 (TIM2_CH1 PWM)：音量越大越亮（挑戰題）
  */
/* USER CODE END Header */
#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define BUF_LEN   256
#define HALF_LEN  128
#define TH_MUL    1
#define TH_ADD    12
/* USER CODE END PD */

ADC_HandleTypeDef  hadc1;
DMA_HandleTypeDef  hdma_adc1;
UART_HandleTypeDef hlpuart1;
TIM_HandleTypeDef  htim2;
TIM_HandleTypeDef  htim3;

/* USER CODE BEGIN PV */
uint16_t adc_buf[BUF_LEN];
volatile uint8_t  process_flag = 0;   // 0=沒事, 1=前半滿, 2=後半滿
volatile uint32_t overrun = 0;

uint32_t noise = 0;
uint32_t th = 0;
uint8_t  led_on = 0;
uint32_t last_toggle = 0;
/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_LPUART1_UART_Init(void);

/* USER CODE BEGIN 0 */
int __io_putchar(int ch)
{
  HAL_UART_Transmit(&hlpuart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

static uint32_t block_loud(const uint16_t *p, uint32_t n)
{
  uint16_t mx = 0, mn = 4095;
  for (uint32_t i = 0; i < n; i++) {
    if (p[i] > mx) mx = p[i];
    if (p[i] < mn) mn = p[i];
  }
  return mx - mn;
}

/* PA5 當一般 GPIO 輸出（開機閃燈測試用） */
static void pa5_as_gpio(void)
{
  GPIO_InitTypeDef g = {0};
  g.Pin   = GPIO_PIN_5;
  g.Mode  = GPIO_MODE_OUTPUT_PP;
  g.Pull  = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &g);
}

/* 直接用暫存器設定 PA5 = TIM2_CH1 PWM（1 kHz） */
static void tim2_pwm_reg_init(void)
{
  RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;      // 開 TIM2 時脈
  RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOAEN;      // 開 GPIOA 時脈
  (void)RCC->APB1ENR1;                       // 等時脈生效

  /* PA5：複用功能模式 (MODER=10)，AF1 = TIM2_CH1 */
  GPIOA->MODER  = (GPIOA->MODER  & ~(3U   << 10)) | (2U << 10);
  GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFU << 20)) | (1U << 20);

  /* TIM2：170 MHz / 170 = 1 MHz，/1000 = 1 kHz */
  TIM2->CR1   = 0;
  TIM2->PSC   = 170 - 1;
  TIM2->ARR   = 1000 - 1;
  TIM2->CCR1  = 0;
  TIM2->CCMR1 = (TIM2->CCMR1 & ~(TIM_CCMR1_OC1M | TIM_CCMR1_CC1S))
              | (6U << TIM_CCMR1_OC1M_Pos)   // PWM mode 1
              | TIM_CCMR1_OC1PE;             // CCR1 預載
  TIM2->CCER |= TIM_CCER_CC1E;               // 打開 CH1 輸出
  TIM2->EGR   = TIM_EGR_UG;                  // 載入設定
  TIM2->CR1  |= TIM_CR1_CEN;                 // 開始計數
}

/* 設定綠燈亮度：0 ~ 1000 = 0% ~ 100% */
static void green_set(uint32_t duty)
{
  TIM2->CCR1 = duty;
}
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_LPUART1_UART_Init();

  /* USER CODE BEGIN 2 */
  printf("\r\n===== Lab6 start =====\r\n");

  /* 測試 1：綠燈當一般 GPIO，閃 3 下 */
  printf("Test1: GREEN blink x3 (GPIO)\r\n");
  pa5_as_gpio();
  for (int i = 0; i < 3; i++) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    HAL_Delay(300);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_Delay(300);
  }

  /* 測試 2：紅燈亮 1 秒 */
  printf("Test2: RED on 1s\r\n");
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
  HAL_Delay(1000);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);

  /* 測試 3：綠燈 PWM 呼吸 3 次 */
  tim2_pwm_reg_init();
  printf("Test3: GREEN PWM fade x3\r\n");
  printf("  CR1=%lx CCMR1=%lx CCER=%lx PSC=%lu ARR=%lu\r\n",
         TIM2->CR1, TIM2->CCMR1, TIM2->CCER, TIM2->PSC, TIM2->ARR);
  printf("  PA5 MODE=%lu AF=%lu\r\n",
         (GPIOA->MODER >> 10) & 3, (GPIOA->AFR[0] >> 20) & 0xF);
  for (int n = 0; n < 3; n++) {
    for (int d = 0; d <= 1000; d += 10)  { green_set(d); HAL_Delay(5); }
    for (int d = 1000; d >= 0; d -= 10)  { green_set(d); HAL_Delay(5); }
  }
  green_set(0);

  /* 保險：強制 DMA 為 Circular、Half Word */
  hdma_adc1.Init.Mode                = DMA_CIRCULAR;
  hdma_adc1.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdma_adc1.Init.MemInc              = DMA_MINC_ENABLE;
  hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_adc1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
  HAL_DMA_Init(&hdma_adc1);

  /* 啟動 ADC + DMA + TIM3 */
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, BUF_LEN);
  HAL_TIM_Base_Start(&htim3);
  printf("Keep quiet 1s...\r\n");

  uint8_t  calibrating = 1;
  uint32_t t_start = HAL_GetTick();
  uint32_t peak = 0;
  uint32_t last_print = 0;
  /* USER CODE END 2 */

  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (process_flag != 0)
    {
      uint8_t f = process_flag;
      process_flag = 0;

      uint16_t *p = (f == 1) ? &adc_buf[0] : &adc_buf[HALF_LEN];

      uint32_t loud = block_loud(p, HALF_LEN);
      uint32_t adc_val = p[HALF_LEN - 1];

      /* 每 0.5 秒印一行 */
      if (loud > peak) peak = loud;
      if (HAL_GetTick() - last_print >= 500) {
        printf("ADC=%lu PEAK=%lu TH=%lu\r\n",
               (unsigned long)adc_val, (unsigned long)peak, (unsigned long)th);
        peak = 0;
        last_print = HAL_GetTick();
      }

      /* 開機 1 秒：量噪音、算門檻 */
      if (calibrating)
      {
        if (loud > noise) noise = loud;
        if (HAL_GetTick() - t_start >= 1000) {
          th = noise * TH_MUL + TH_ADD;
          calibrating = 0;
          printf("NOISE=%lu TH=%lu  -> now clap!\r\n",
                 (unsigned long)noise, (unsigned long)th);
        }
        continue;
      }

      /* 基本題：拍手切換紅燈 PC6 */
      if (loud > th && HAL_GetTick() - last_toggle > 300)
      {
        led_on = !led_on;
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, led_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
        last_toggle = HAL_GetTick();
        printf("CLAP! LOUD=%lu RED=%s\r\n", (unsigned long)loud, led_on ? "ON" : "OFF");
      }

      /* 挑戰題：綠燈依音量 0% / 33% / 66% / 100% */
      if      (loud >= 3 * th) green_set(1000);
      else if (loud >= 2 * th) green_set(666);
      else if (loud >= th)     green_set(333);
      else                     green_set(0);
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM            = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN            = 85;
  RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) Error_Handler();
}

static void MX_ADC1_Init(void)
{
  ADC_MultiModeTypeDef   multimode = {0};
  ADC_ChannelConfTypeDef sConfig   = {0};

  hadc1.Instance                   = ADC1;
  hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  hadc1.Init.GainCompensation      = 0;
  hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait      = DISABLE;
  hadc1.Init.ContinuousConvMode    = DISABLE;
  hadc1.Init.NbrOfConversion       = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv      = ADC_EXTERNALTRIG_T3_TRGO;
  hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
  hadc1.Init.OversamplingMode      = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();

  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK) Error_Handler();

  sConfig.Channel      = ADC_CHANNEL_1;
  sConfig.Rank         = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
  sConfig.SingleDiff   = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset       = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) Error_Handler();
}

static void MX_LPUART1_UART_Init(void)
{
  hlpuart1.Instance                    = LPUART1;
  hlpuart1.Init.BaudRate               = 115200;
  hlpuart1.Init.WordLength             = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits               = UART_STOPBITS_1;
  hlpuart1.Init.Parity                 = UART_PARITY_NONE;
  hlpuart1.Init.Mode                   = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK) Error_Handler();
  if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) Error_Handler();
  if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) Error_Handler();
  if (HAL_UARTEx_DisableFifoMode(&hlpuart1) != HAL_OK) Error_Handler();
}

/* TIM2 改由 tim2_pwm_reg_init() 用暫存器設定，這裡只留 handle */
static void MX_TIM2_Init(void)
{
  htim2.Instance = TIM2;
}

static void MX_TIM3_Init(void)
{
  TIM_ClockConfigTypeDef  sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig      = {0};

  __HAL_RCC_TIM3_CLK_ENABLE();

  htim3.Instance               = TIM3;
  htim3.Init.Prescaler         = 170 - 1;     // 1 MHz
  htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim3.Init.Period            = 125 - 1;     // 8 kHz
  htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK) Error_Handler();

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK) Error_Handler();

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK) Error_Handler();
}

static void MX_DMA_Init(void)
{
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin   = GPIO_PIN_6;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1) {
    if (process_flag != 0) overrun++;
    process_flag = 1;
  }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1) {
    if (process_flag != 0) overrun++;
    process_flag = 2;
  }
}
/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif
