# Lab2：PC9 按鈕切換 LED 顏色與閃爍速度

## 專案目錄結構

```
lab2/
├── Core/                              # 專案核心程式碼
│   ├── Inc/                           # 標頭檔目錄
│   │   ├── main.h                     # 全域定義、腳位別名與函式原型宣告
│   │   ├── stm32g4xx_hal_conf.h       # STM32 HAL 庫功能模組開啟／關閉配置
│   │   └── stm32g4xx_it.h             # 中斷服務函式標頭檔
│   ├── Src/                           # C 語言原始碼目錄
│   │   ├── main.c                     # 主程式：LED 閃爍（HAL_GetTick）、按鈕輪詢與去彈跳
│   │   ├── stm32g4xx_hal_msp.c        # MCU 底層硬體初始化
│   │   ├── stm32g4xx_it.c             # 中斷向量處理（包含 EXTI9_5 中斷入口）
│   │   ├── syscalls.c                 # 系統呼叫（CubeIDE 自動產生）
│   │   ├── sysmem.c                   # 記憶體配置（CubeIDE 自動產生）
│   │   └── system_stm32g4xx.c         # 系統時脈與 CMSIS 底層啟動配置
│   └── Startup/
│       └── startup_stm32g474retx.s    # 組合語言啟動檔（向量表、Reset_Handler）
├── Drivers/                           # 硬體驅動庫
│   ├── CMSIS/                         # ARM Cortex-M 核心標準抽象層
│   └── STM32G4xx_HAL_Driver/          # ST 官方 HAL 驅動
├── lab2.ioc                           # STM32CubeMX 周邊組態設定檔
├── STM32G474RETX_FLASH.ld             # Linker Script（Flash 執行）
├── STM32G474RETX_RAM.ld               # Linker Script（RAM 執行）
└── README.md                          # 專案說明文件
```
