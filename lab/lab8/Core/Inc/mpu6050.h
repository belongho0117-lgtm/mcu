/**
  ******************************************************************************
  * @file    mpu6050.h
  * @brief   MPU6050 加速度計驅動（I2C 中斷版，非阻塞）— Lab8 提供程式
  ******************************************************************************
  */
#ifndef MPU6050_H
#define MPU6050_H

#include "main.h"

#define MPU6050_ROLL_SIGN   (+1.0f)   /* 左右亮反了就改成 (-1.0f) */

/* 開始：記住用哪個 I2C，從「喚醒」開始跑 */
void    MPU6050_Start(I2C_HandleTypeDef *hi2c);

/* 放在 while(1) 裡一直呼叫：內部狀態機負責喚醒、確認身分、每 50 ms 讀一次 */
void    MPU6050_Process(void);

/* 有新角度時回傳 1，並把左右傾斜角度（度）寫進 *roll；沒有新資料回傳 0 */
uint8_t MPU6050_GetRoll(float *roll);

/* 找不到 MPU6050（NACK 或 WHO_AM_I 不對）時回傳 1；2 秒後會自動重試 */
uint8_t MPU6050_IsError(void);

#endif /* MPU6050_H */
