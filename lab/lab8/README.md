# Lab8：I2C 讀取 MPU6050 加速度計，傾斜控制三顆 LED

用 I2C1（中斷版、非阻塞）每 50 ms 讀一次 MPU6050 的加速度，用 Y、Z 軸算出左右傾斜角度（roll）。
板子往左傾亮左燈、往右傾亮右燈、放平亮中間燈；找不到 MPU6050 時三顆燈一起閃，2 秒後自動重試。

## 接線

| MPU6050 | NUCLEO-G474RE | 說明 |
|---|---|---|
| SCL | PB8 | I2C1_SCL |
| SDA | PB9 | I2C1_SDA |
| VCC／GND | 3.3V／GND | 電源 |

| LED | 腳位 |
|---|---|
| 左（LED_L） | PC8 |
| 中（LED_M） | PC6 |
| 右（LED_R） | PC5 |

## 專案目錄結構

```
lab8/
├── Core/                              # 專案核心程式碼
│   ├── Inc/                           # 標頭檔目錄
│   │   ├── main.h                     # 全域定義、腳位別名與函式原型宣告
│   │   ├── mpu6050.h                  # MPU6050 驅動介面（Start／Process／GetRoll／IsError）
│   │   ├── stm32g4xx_hal_conf.h       # STM32 HAL 庫功能模組開啟／關閉配置
│   │   └── stm32g4xx_it.h             # 中斷服務函式標頭檔
│   ├── Src/                           # C 語言原始碼目錄
│   │   ├── main.c                     # 主程式：左／中／右狀態機（遲滯 3°／2°）、出錯閃燈
│   │   ├── mpu6050.c                  # MPU6050 驅動：喚醒 → WHO_AM_I → 每 50 ms 讀加速度的狀態機
│   │   ├── stm32g4xx_hal_msp.c        # MCU 底層硬體初始化（I2C1 腳位與時脈）
│   │   ├── stm32g4xx_it.c             # 中斷向量處理（I2C1 事件／錯誤、SysTick）
│   │   ├── syscalls.c                 # 系統呼叫（CubeIDE 自動產生）
│   │   ├── sysmem.c                   # 記憶體配置（CubeIDE 自動產生）
│   │   └── system_stm32g4xx.c         # 系統時脈與 CMSIS 底層啟動配置
│   └── Startup/
│       └── startup_stm32g474retx.s    # 組合語言啟動檔（向量表、Reset_Handler）
├── Drivers/                           # 硬體驅動庫
│   ├── CMSIS/                         # ARM Cortex-M 核心標準抽象層
│   └── STM32G4xx_HAL_Driver/          # ST 官方 HAL 驅動
├── Lab8.ioc                           # STM32CubeMX 周邊組態設定檔（I2C1、LED GPIO）
├── STM32G474RETX_FLASH.ld             # Linker Script（Flash 執行）
├── STM32G474RETX_RAM.ld               # Linker Script（RAM 執行）
└── README.md                          # 專案說明文件
```

## MPU6050 讀取流程

| 步驟 | 動作 | 暫存器 |
|---|---|---|
| 1 | 寫 0x00 喚醒 | PWR_MGMT_1（0x6B） |
| 2 | 等 100 ms 後讀身分，要等於 0x68 | WHO_AM_I（0x75） |
| 3 | 每 50 ms 從 0x3B 連讀 6 bytes（X、Y、Z 加速度） | ACCEL_XOUT_H（0x3B） |
| 出錯 | NACK 或 WHO_AM_I 不對 → 等 2 秒從步驟 1 重來 | |

傳輸用 `HAL_I2C_Mem_Write_IT`／`HAL_I2C_Mem_Read_IT`，完成時由 `HAL_I2C_MemTxCpltCallback`／`HAL_I2C_MemRxCpltCallback` 通知，主迴圈不會卡住。

## 傾斜判斷

roll = atan2(ay, az) × 57.2958（度）

- 傾斜超過 3° 才切到左或右，回到 2° 以內才回中間（遲滯，避免在邊界亂跳）。
- 左右亮反了，把 `mpu6050.h` 的 `MPU6050_ROLL_SIGN` 改成 `(-1.0f)`。
