# Lab7：DAC 訊號產生器（TIM6 中斷查表）

TIM6 每 0.2 ms 中斷一次，每次從 100 點波形表送一點給 DAC1，PA4 輸出 50 Hz 波形。
按 USER 鍵（B1／PC13）依序切換：正弦 → 三角 → 方波 → 正弦。

## 專案目錄結構

```
lab7/
├── Core/                              # 專案核心程式碼
│   ├── Inc/                           # 標頭檔目錄
│   │   ├── main.h                     # 全域定義、腳位別名與函式原型宣告
│   │   ├── stm32g4xx_hal_conf.h       # STM32 HAL 庫功能模組開啟／關閉配置
│   │   └── stm32g4xx_it.h             # 中斷服務函式標頭檔
│   ├── Src/                           # C 語言原始碼目錄
│   │   ├── main.c                     # 主程式：建三張波形表、TIM6 Callback 送 DAC、PC13 切換波形
│   │   ├── stm32g4xx_hal_msp.c        # MCU 底層硬體初始化（PA4 設為類比、DAC1／TIM6 時脈）
│   │   ├── stm32g4xx_it.c             # 中斷向量處理（TIM6_DAC、EXTI15_10、SysTick）
│   │   ├── syscalls.c                 # 系統呼叫（CubeIDE 自動產生）
│   │   ├── sysmem.c                   # 記憶體配置（CubeIDE 自動產生）
│   │   └── system_stm32g4xx.c         # 系統時脈與 CMSIS 底層啟動配置
│   └── Startup/
│       └── startup_stm32g474retx.s    # 組合語言啟動檔（向量表、Reset_Handler）
├── Drivers/                           # 硬體驅動庫
│   ├── CMSIS/                         # ARM Cortex-M 核心標準抽象層
│   └── STM32G4xx_HAL_Driver/          # ST 官方 HAL 驅動
├── Lab7_DAC.ioc                       # STM32CubeMX 周邊組態設定檔（DAC1、TIM6、PC13 EXTI）
├── STM32G474RETX_FLASH.ld             # Linker Script（Flash 執行）
├── STM32G474RETX_RAM.ld               # Linker Script（RAM 執行）
└── README.md                          # 專案說明文件
```

## CubeMX 設定

| 周邊 | 設定 | 說明 |
|---|---|---|
| TIM6 | Prescaler = 169、Counter Period = 199 | 170 MHz ÷ 170 ÷ 200 = 5 kHz（每 0.2 ms 中斷一次） |
| NVIC | TIM6 global interrupt | 沒勾的話 Callback 不會被呼叫 |
| DAC1 | OUT1 = external pin only、Output Buffer = Enable、Trigger = None | 程式用 `HAL_DAC_SetValue` 寫值，寫進去馬上輸出 |
| PC13 | GPIO_EXTI13、EXTI line[15:10] interrupt | USER 鍵切換波形 |

## 輸出頻率

輸出頻率 = 中斷頻率 ÷ 表的點數 = 5000 Hz ÷ 100 = **50 Hz**

波形表範圍 248～3848（中心 2048 ≈ 1.65 V），上下留空間避免波峰波谷被削平。
