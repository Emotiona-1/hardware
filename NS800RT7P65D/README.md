# NS800RT7P65D 执行端固件

机器人**执行端**的裸机 C 固件：接收视觉节点（K230）通过串口下发的动作指令，驱动步进电机、
总线舵机、直流电机与振动电机完成 **振动 / 吹风 / 触摸** 三种作业动作，并在完成后回到原点。

> 本目录只包含**用户自己写的驱动层与应用层代码**，不含 MCU 厂商 SDK 与工程文件，因此
> **无法直接编译**，详见 [第 3 节](#3-重要前提本仓库只含用户代码)。

---

## 1. 功能概述

执行端对外只有一条串口链路（UART2，115200 8N1），协议是**按行分隔的 ASCII 文本**：

| 收到 | 含义 | 动作 |
| --- | --- | --- |
| `one` | 选择作业模式 1：振动 | 进入 `MODE_VIBRATION` |
| `two` | 选择作业模式 2：吹风 | 进入 `MODE_BLOW` |
| `three` | 选择作业模式 3：触摸 | 进入 `MODE_TOUCH` |
| `角度,Y`（如 `30,120`） | 目标坐标 | 步进电机转到 `角度`，舵机移动到 `Y`，执行当前模式动作，然后回原点 |
| 其他 | — | 忽略（模式未选时收到坐标会回 `noMode`） |

回复只有两种：`noMode\r\n`（还没收到模式指令就来了坐标）、`workOK\r\n`（一次作业完成）。

坐标指令里的 `Y` 可以省略（例如只发 `30`），此时会沿用上一次的 `Y` 值；`角度` 被限幅到
`-90 ~ +90`。

---

## 2. 目录结构

```
NS800RT7P65D/
├── Hardware/                  # 驱动层（全部为自写代码）
│   ├── delay.c/.h             # 基于 SysTick 的微秒/毫秒延时
│   ├── LED.c/.h               # 2 路板载 LED
│   ├── Key.c/.h               # 2 路按键（阻塞式消抖）
│   ├── OLED.c/.h              # 0.96" OLED，软件 I2C，含字符/数字/hex/bin 显示
│   ├── OLED_Font.h            # 8x16 ASCII 字库（第三方资源，见第 10 节）
│   ├── PWM.c/.h               # EPWM1 双通道，20 kHz，占空比 0~100
│   ├── DCMotor.c/.h           # 直流电机：方向 + 占空比（内部调用 PWM）
│   ├── motor.c/.h             # 步进电机：PUL/DIR/ENA 三线，软件脉冲
│   ├── track.c/.h             # 步进电机「角度 → 脉冲数」换算与闭环（开环记忆）
│   ├── Servo.c/.h             # 总线舵机：UART1 上的一帧式协议（0x55 0x55 帧头）
│   ├── Serial.c/.h            # UART1 / UART2 收发 + 接收中断
│   ├── Ultrasonic.c/.h        # 超声波测距：ECAP 输入捕获 + XBAR 内部互联
│   ├── vibration_motor.c/.h   # 振动电机：GPIO 开关 或 EPWM8 强度调节
│   └── Control.c/.h           # 业务主逻辑：解析指令、编排动作、回原点
└── User/
    ├── main.c                 # 入口：System_Init() 自检 + Control_MainLoop() 主循环
    ├── ns800rt7p65d_it.c      # 异常/中断向量（默认空实现，实际 ISR 由驱动注册）
    └── ns800rt7p65d_it.h
```

---

## 3. 重要前提：本仓库只含用户代码

代码里出现的 `NS800RTxxxx.h`、`Device_init()`、`Device_unlockPeriphReg()`、
`Interrupt_initModule()`、`GPIO_*`、`EPWM_*`、`ECAP_*`、`XBAR_*`、`UART_*`、`RCC_*` 等
符号全部来自 **MCU 厂商提供的 SDK**，本仓库不包含：

- ❌ 厂商 SDK / 头文件（`NS800RTxxxx.h` 及其驱动库）
- ❌ 启动文件、链接脚本、系统初始化
- ❌ IDE 工程文件（`.uvprojx` / `.ewp` 等）
- ❌ 任何编译产物

**使用方法**：在厂商 SDK 的示例工程中新建 `Hardware` 与 `User` 两个分组，把本目录的文件
加入编译即可（`User/main.c` 替换示例工程里的 main）。具体 SDK 名称与获取方式
**需作者确认**。

---

## 4. 引脚分配表

以下引脚全部从代码中的宏定义读出（`文件:宏名`），**未在表中出现的引脚均未被本工程使用**。

| 功能 | 引脚 | 复用 | 外设 / 模块 | 定义位置 |
| --- | --- | --- | --- | --- |
| 直流电机 PWM 正转 | PA0 | ALT1 | EPWM1 A | `Hardware/PWM.c` |
| 直流电机 PWM 反转 | PA1 | ALT1 | EPWM1 B | `Hardware/PWM.c` |
| 步进电机 脉冲 PUL | PA2 | ALT0（GPIO） | — | `Hardware/motor.h` |
| 步进电机 方向 DIR | PA3 | ALT0（GPIO） | — | `Hardware/motor.h` |
| 步进电机 使能 ENA | PA4 | ALT0（GPIO） | — | `Hardware/motor.h` |
| 超声波 TRIG | PA5 | ALT0（GPIO） | — | `Hardware/Ultrasonic.c` |
| 直流电机驱动 NSLEEP | PA8 | ALT0（GPIO） | — | `Hardware/DCMotor.h` |
| 总线舵机 TX | PA12 | ALT6 | UART1 TX | `Hardware/Servo.c` |
| 总线舵机 RX | PA13 | ALT6 | UART1 RX | `Hardware/Servo.c` |
| 振动电机（开关模式） | PA14 | ALT0（GPIO） | — | `Hardware/vibration_motor.h` |
| 振动电机（PWM 强度模式） | PA14 | ALT1 | EPWM8 A | `Hardware/vibration_motor.h` |
| 超声波 ECHO | PA16 | ALT0 | ECAP1（经 XBAR INPUT7） | `Hardware/Ultrasonic.c` |
| 上位机串口 RX（接 K230 TX） | PB6 | ALT1 | UART2 TX | `Hardware/Serial.c` |
| 上位机串口 TX（接 K230 RX） | PB7 | ALT1 | UART2 RX | `Hardware/Serial.c` |
| OLED SCL | PB14 | ALT0（GPIO 开漏） | 软件 I2C | `Hardware/OLED.c` |
| OLED SDA | PB15 | ALT0（GPIO 开漏） | 软件 I2C | `Hardware/OLED.c` |
| LED1 | PB0 | ALT0（GPIO） | — | `Hardware/LED.c` |
| LED2 | PB1 | ALT0（GPIO） | — | `Hardware/LED.c` |
| KEY1 | PB2 | ALT0（GPIO，上拉） | — | `Hardware/Key.c` |
| KEY2 | PB12 | ALT0（GPIO，上拉） | — | `Hardware/Key.c` |

> ⚠️ **UART1 被两处代码同时初始化**：`Serial.c` 的 `Serial1_Init()`（PA12/PA13 @115200）与
> `Servo.c` 的 `SERVO_Init()`（同样是 PA12/PA13 @115200）配置的是**同一个 UART1**。
> `main.c` 只调用了 `SERVO_Init()`，因此 `Serial1_*` 这一组函数目前是**冗余/预留**的，
> 若将来要同时使用，需要先合并这两处初始化。

---

## 5. 初始化顺序

`User/main.c` 的 `System_Init()` 顺序如下（顺序有依赖，不建议调整）：

```c
Device_init();                    // 厂商 SDK：器件初始化
Device_unlockPeriphReg();         // 解锁外设寄存器写保护
Interrupt_initModule();           // 中断控制器初始化
Interrupt_initVectorTable();      // 中断向量表

delay_init();                     // SysTick 延时（后续所有 Delay_* 依赖它）
Motor_Init();  Motor_Enable();    // 步进电机 GPIO，并使能驱动器
Serial2_Init();                   // 与 K230 通信的 UART2（PB6/PB7，115200）
SERVO_Init();                     // 总线舵机 UART1（PA12/PA13，115200）
VibrationMotor_Init();            // 振动电机 GPIO
DCMotor_Init();                   // 直流电机 NSLEEP + EPWM1（内部调用 PWM_Init）
Ultrasonic_Init();                // ECAP1 + XBAR + 捕获中断
```

随后 `System_Init()` 会做一次上电自检：依次给 3 个舵机上电（`SERVO_LoadOrUnload`）、
各做一次行程动作、振动电机开 1 秒、超声波测一次距离、直流电机以 50% 转 1 秒。

`main()` 之后进入死循环 `while (1) { Control_MainLoop(); }` —— 所有业务都是在这一层
**轮询 + 阻塞延时**完成的，没有 RTOS。

---

## 6. 驱动 API 速查

### 6.1 延时 `delay.h`

| 函数 / 宏 | 说明 |
| --- | --- |
| `delay_init()` | 读取 `SystemCoreClock`，初始化 SysTick |
| `delay_us(nus)` / `Delay_us(nus)` | 微秒级忙等（`Delay_us` 是宏，等价于小写版本） |
| `delay_ms(nms)` / `Delay_ms(nms)` | 毫秒级忙等，内部循环调用 `delay_us(1000)` |

### 6.2 步进电机 `motor.h` / `track.h`

| 函数 | 说明 |
| --- | --- |
| `Motor_Init()` | 配置 PUL/DIR/ENA 为输出，默认 ENA 拉高（**禁用**） |
| `Motor_Enable()` / `Motor_Disable()` | ENA 拉低使能 / 拉高禁用 |
| `Motor_SetDirection(DIR_CW / DIR_CCW)` | 设置 DIR 电平 |
| `Motor_OnePulse()` / `Motor_OnePulse_Slow()` | 单个脉冲（高 50 µs / 慢速 600 µs） |
| `Motor_RunSteps(n)` / `Motor_RunSteps_Slow(n)` | 连续发 n 个脉冲 |
| `Motor_TrackX(target_angle)` | **按角度**运动：内部按 `1600 脉冲/圈 × 3` 细分换算，带 ±2° 死区与当前位置记忆 |

### 6.3 直流电机 `DCMotor.h` / `PWM.h`

| 函数 | 说明 |
| --- | --- |
| `PWM_Init()` | EPWM1 初始化，20 kHz，占空比满量程 100 |
| `PWM_SetCompare3(v)` / `PWM_SetCompare4(v)` | 设置 A / B 通道占空比（0~100，自动限幅） |
| `DCMotor_Init()` | 配置 NSLEEP 并拉高（唤醒驱动）+ 调用 `PWM_Init()` |
| `DCMotor_SetSpeed(speed)` | `speed` 取 `-100 ~ +100`：正数走 A 通道，负数走 B 通道（H 桥换向） |

### 6.4 总线舵机 `Servo.h`

一帧格式：`0x55 0x55 | ID | 长度 | 指令 | 参数... | 校验和`，校验和 = `~(sum(除帧头外所有字节)) & 0xFF`。

| 函数 | 指令码 | 说明 |
| --- | --- | --- |
| `SERVO_Init()` | — | UART1 @115200，8N1，无 FIFO |
| `SERVO_MoveTimeWrite(id, angle, time_ms)` | 1 | 在 `time_ms` 内转到 `angle` |
| `SERVO_MoveTimeWaitWrite(id, angle, time_ms)` | 7 | 同上，但等待上次动作完成后再执行 |
| `SERVO_MoveStart(id)` / `SERVO_MoveStop(id)` | 11 / 12 | 启动 / 停止 |
| `SERVO_LoadOrUnload(id, load)` | 31 | 舵机扭力加载（1）/ 卸载（0） |
| `SERVO_AngleLimitWrite(id, min, max)` | 20 | 写入角度限位 |
| `SERVO_IdWrite(id, new_id)` | 13 | 修改舵机 ID |
| `SERVO_SetAngleOffset(id, offset)` | 17 | 角度偏差调整 |
| `SERVO_LedCtrl(id, on_off)` | 33 | 舵机自带 LED |

本工程使用 3 个舵机 ID：`1`、`2`（姿态，见 `Control_ServoMoveByY`）、`3`（伸缩机构，
见 `Control_ServoMoveByDistance`，行程 0~1000，按 `200 单位/cm`、上限 5 cm 换算）。

### 6.5 串口 `Serial.h`

| 函数 | 说明 |
| --- | --- |
| `Serial1_Init()` / `Serial2_Init()` | UART1 @115200（PA12/PA13）、UART2 @115200（PB6/PB7），均开启接收中断 |
| `Serial1_SendByte/SendString/Printf` | 发送（`Printf` 内部用 `vsprintf`，缓冲区 100 字节） |
| `Serial1_GetRxFlag()` | 查询是否有新字节，**读取即清标志** |
| `Serial1_GetRxData()` | 取出最近收到的字节 |

（`Serial2_*` 与 `Serial1_*` 同名同构，仅外设不同。）

### 6.6 超声波 `Ultrasonic.h`

| 函数 | 说明 |
| --- | --- |
| `Ultrasonic_Init()` | TRIG 输出、ECHO 输入，ECHO 经 XBAR 连到 ECAP1；ECAP 单次捕获模式，上升沿/下降沿各捕一次 |
| `Ultrasonic_GetDistance()` | 返回距离（cm，`float`）。超时（约 60000 次轮询）返回 `0.0f`；换算按 `58 µs/cm` |

### 6.7 振动电机 `vibration_motor.h`

| 函数 | 说明 |
| --- | --- |
| `VibrationMotor_Init()` | PA14 配为 GPIO 输出，默认关 |
| `VibrationMotor_On()` / `Off()` / `Toggle()` | 直接开关 |
| `VibrationMotor_Vibrate(ms)` | 开 `ms` 毫秒后自动关 |
| `VibrationMotor_PWM_Init(period, prescaler)` | 把 PA14 切到 EPWM8，用于**强度调节**（本工程 `main.c` 未调用） |
| `VibrationMotor_SetIntensity(v)` | `v` 取 0~1000 |

### 6.8 人机交互 `LED.h` / `Key.h` / `OLED.h`

| 函数 | 说明 |
| --- | --- |
| `LED_Init()` / `LED1_ON/OFF/Turn()` / `LED2_ON/OFF/Turn()` | 低电平点亮 |
| `Key_Init()` / `Key_GetNum()` | 返回 1 或 2；**阻塞式**消抖，会一直等到松手 |
| `OLED_Init()` / `OLED_Clear()` | 软件 I2C 初始化 / 清屏 |
| `OLED_ShowChar/ShowString/ShowNum/ShowSignedNum/ShowHexNum/ShowBinNum` | 按「行、列」显示，行 1~4，列 1~16 |

> `LED_*`、`Key_*`、`OLED_*` 三组驱动 `main.c` **均未调用**，属于预留给后续功能的代码。

---

## 7. 业务逻辑 `Control.c`

### 7.1 指令解析

`Control_MainLoop()` 每轮从 UART2 取一个字节，攒到 32 字节的行缓冲里，遇到 `\n` 或 `\r`
就调用 `Control_ProcessLine()`：

- `one` / `two` / `three` → 设置 `s_mode`（不执行动作，等坐标指令）
- `角度,Y` → `sscanf("%hd,%hd")`；只解析到 1 个值时 `Y` 沿用上次的值
- 其他内容 → 丢弃

### 7.2 一次完整作业的时序

`Control_OnCoordinate()`：

1. 模式为 `MODE_NONE` → 回 `noMode\r\n` 并返回（**不会动作**）
2. `Motor_TrackX(angle)` —— 步进电机转到目标角度
3. `Control_ServoMoveByY(y)` —— 舵机 1/2 移动到目标姿态，随后 `Delay_ms(3000)`
4. `Control_RunModeAction()` —— 执行当前模式的作业动作
5. `Control_ReturnHome()` —— 伸缩舵机回 0 → 舵机 1 回 350、舵机 2 回 400 → 步进回 0
6. 回 `workOK\r\n`

### 7.3 三种作业模式

| 模式 | 触发 | 动作（`Control.c`） |
| --- | --- | --- |
| `MODE_VIBRATION` | `one` | 等 2 s → 超声波测距 → 伸缩舵机按距离伸出 → 振动电机开 1 s 后关 |
| `MODE_BLOW` | `two` | 等 2 s → 直流电机 100% 占空比转 3 s → 停 |
| `MODE_TOUCH` | `three` | 等 2 s → 超声波测距 → 伸缩舵机按距离伸出 |

坐标系与换算（写死在代码里，属于调试后的经验值）：

- 姿态：`舵机1 = 350 - y×350/200`，`舵机2 = 400 - 2y + 110`，`y` 限幅 `0~200`
- 伸缩：`位置 = 距离(cm) × 200`，距离限幅 `0~5 cm`，即位置 `0~1000`，运动 1000 ms、保持 5000 ms

---

## 8. 与 K230 的对接

| 项目 | 本工程（NS800RT7P65D） | K230 端 |
| --- | --- | --- |
| 串口 | UART2：PB6(TX)/PB7(RX) | UART1：GPIO3(TX)/GPIO4(RX) |
| 波特率 | 115200 8N1 | 115200 8N1 |
| 接线 | PB6 → K230 GPIO4(RX)，PB7 ← K230 GPIO3(TX)，共地 | — |
| 上行 | `noMode` / `workOK` | 靠 `workOK` 子串判断一次作业结束 |
| 下行 | `one` / `two` / `three` / `角度,Y` | 由 MQTT 指令 `1`/`2`/`3` 与识别坐标触发 |

细节见 [../K230/README.md](../K230/README.md)。

---

## 9. 编译与烧录

1. 从 MCU 厂商获取 **NS800RT7P65D 的 SDK / 示例工程**（本仓库不含，见第 3 节）。
2. 在工程中新建 `Hardware`、`User` 两个分组，加入本目录下所有 `.c` 文件，并把头文件路径
   指向 `Hardware/`、`User/`。
3. 用工程原本的启动文件与链接脚本编译（本仓库没有提供的文件一律沿用 SDK 的）。
4. 通过调试器（J-Link / DAP-Link 等）烧录后复位即可运行。

> 具体 IDE（Keil MDK / IAR / Eclipse 等）与烧录方式 **需作者确认**。

---

## 10. 已知问题与注意事项

1. **无法独立编译**：缺少 SDK、启动文件与工程文件（见第 3 节）。
2. **UART1 重复初始化**：`Serial1_Init()` 与 `SERVO_Init()` 操作同一组引脚，目前只有后者被调用。
3. **阻塞式控制流**：作业时序大量使用 `Delay_ms(2000/3000/5000)`，期间不响应串口新指令
   （指令会在缓冲里排队，但动作无法打断）。
4. **步进电机是开环的**：`track.c` 用 `s_current_angle` 记忆位置，掉电或失步后归零会不准；
   每个脉冲之间有 `delay_us(500)` 的软件延时，高速运动能力有限。
5. **超声波可能返回 0**：超时返回 `0.0f`，此时伸缩舵机会停在 0 位（`Control_ServoMoveByDistance`
   把 0 当作有效距离处理），如需区分「测距失败」需自行加判断。
6. **`OLED_Font.h` 是第三方字库**：8×16 ASCII 点阵在中文字教程圈广泛流传（常见于「江协科技」
   等 STM32/OLED 教程），本仓库未找到明确许可证声明，若原作者有异议请提 Issue 处理。
7. **待作者确认**：MCU 厂商与 SDK 名称、总线舵机型号（协议形似 LX-16A 系列）、
   直流电机驱动芯片（NSLEEP 引脚形似 TB6612/DRV8871）、坐标 `Y` 的物理单位、
   以及 `one/two/three` 三种作业模式的正式名称。
