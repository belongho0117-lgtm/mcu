/**
  ******************************************************************************
  * @file    mpu6050.c
  * @brief   MPU6050 加速度計驅動（I2C 中斷版，非阻塞）— Lab8 提供程式
  *
  * 只用到兩個 HAL I2C 函式：
  *   HAL_I2C_Mem_Write_IT：送資料（寫某一格暫存器）
  *   HAL_I2C_Mem_Read_IT ：讀資料（從某一格開始連續讀）
  * 傳完由 HAL 呼叫下面的 Callback 通知。
  ******************************************************************************
  */
#include "mpu6050.h"
#include <math.h>

#define MPU_ADDR          (0x68 << 1)  /* MPU6050 的 I2C 位址，HAL 要左移一位 */
#define REG_PWR_MGMT_1    0x6B         /* 電源設定：寫 0x00 喚醒 */
#define REG_WHO_AM_I      0x75         /* 身分確認：應讀到 0x68 */
#define REG_ACCEL_XOUT_H  0x3B         /* 加速度資料起始格，連讀 6 格 */
#define READ_PERIOD_MS    50           /* 每 50 ms 讀一次 */
#define RETRY_MS          2000         /* 出錯後多久重試 */

typedef enum {
  ST_IDLE_OFF,
  ST_WAKE, ST_WAIT_WAKE,
  ST_WHOAMI, ST_WAIT_WHOAMI,
  ST_READY, ST_WAIT_ACCEL,
  ST_ERROR
} mpu_state_t;

static I2C_HandleTypeDef *mpu_i2c = NULL;
static volatile mpu_state_t state = ST_IDLE_OFF;
static volatile uint8_t i2c_done = 0;   /* 傳完：Callback 設 1 */
static volatile uint8_t i2c_err  = 0;   /* 出錯：Callback 設 1 */

static uint8_t  wake_val = 0x00;
static uint8_t  who_am_i = 0;
static uint8_t  accel_buf[6];           /* X高 X低 Y高 Y低 Z高 Z低 */
static uint32_t last_tick = 0;
static float    roll_val = 0.0f;
static uint8_t  roll_new = 0;

static int16_t to_int16(uint8_t hi, uint8_t lo)
{
  return (int16_t)((hi << 8) | lo);
}

static void go_error(void)
{
  last_tick = HAL_GetTick();
  state = ST_ERROR;
}

void MPU6050_Start(I2C_HandleTypeDef *hi2c)
{
  mpu_i2c = hi2c;
  state = ST_WAKE;
}

void MPU6050_Process(void)
{
  if (i2c_err) {                       /* 沒人回 ACK 等錯誤 */
    i2c_err = 0;
    go_error();
  }

  switch (state) {
  case ST_WAKE:                        /* 【送資料】寫一格：0x6B = 0x00，叫醒 MPU6050 */
    i2c_done = 0;
    if (HAL_I2C_Mem_Write_IT(mpu_i2c, MPU_ADDR, REG_PWR_MGMT_1,
                             I2C_MEMADD_SIZE_8BIT, &wake_val, 1) == HAL_OK)
      state = ST_WAIT_WAKE;
    else
      go_error();
    break;

  case ST_WAIT_WAKE:                   /* 等寫完，再等 100 ms 讓它穩定 */
    if (i2c_done) {
      last_tick = HAL_GetTick();
      state = ST_WHOAMI;
    }
    break;

  case ST_WHOAMI:                      /* 【讀資料】讀一格：0x75，確認通訊正常 */
    if (HAL_GetTick() - last_tick >= 100) {
      i2c_done = 0;
      if (HAL_I2C_Mem_Read_IT(mpu_i2c, MPU_ADDR, REG_WHO_AM_I,
                              I2C_MEMADD_SIZE_8BIT, &who_am_i, 1) == HAL_OK)
        state = ST_WAIT_WHOAMI;
      else
        go_error();
    }
    break;

  case ST_WAIT_WHOAMI:                 /* 讀到 0x68 才往下 */
    if (i2c_done) {
      last_tick = HAL_GetTick();
      if (who_am_i == 0x68) state = ST_READY;
      else go_error();
    }
    break;

  case ST_READY:                       /* 【讀資料】每 50 ms 從 0x3B 連讀 6 格 */
    if (HAL_GetTick() - last_tick >= READ_PERIOD_MS) {
      last_tick = HAL_GetTick();
      i2c_done = 0;
      if (HAL_I2C_Mem_Read_IT(mpu_i2c, MPU_ADDR, REG_ACCEL_XOUT_H,
                              I2C_MEMADD_SIZE_8BIT, accel_buf, 6) == HAL_OK)
        state = ST_WAIT_ACCEL;
      else
        go_error();
    }
    break;

  case ST_WAIT_ACCEL:                  /* 讀完：用 Y、Z 算左右傾斜角度 */
    if (i2c_done) {
      int16_t ay = to_int16(accel_buf[2], accel_buf[3]);
      int16_t az = to_int16(accel_buf[4], accel_buf[5]);
      roll_val = MPU6050_ROLL_SIGN * atan2f((float)ay, (float)az) * 57.2958f;
      roll_new = 1;
      state = ST_READY;
    }
    break;

  case ST_ERROR:                       /* 等 2 秒後從喚醒重來 */
    if (HAL_GetTick() - last_tick >= RETRY_MS) state = ST_WAKE;
    break;

  default:
    break;
  }
}

uint8_t MPU6050_GetRoll(float *roll)
{
  if (!roll_new) return 0;
  roll_new = 0;
  *roll = roll_val;
  return 1;
}

uint8_t MPU6050_IsError(void)
{
  return (state == ST_ERROR);
}

/* ---- HAL 傳完／出錯時會呼叫這些函式 ---- */
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)   /* 送完 */
{
  if (hi2c == mpu_i2c) i2c_done = 1;
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)   /* 讀完 */
{
  if (hi2c == mpu_i2c) i2c_done = 1;
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)       /* 出錯（例如 NACK） */
{
  if (hi2c == mpu_i2c) i2c_err = 1;
}
