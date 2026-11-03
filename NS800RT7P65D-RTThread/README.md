# NS800RT7P65D 执行端 · RT-Thread 版本

这是执行端固件的 **RT-Thread 版**，运行在比赛指定的 **NS800RT7P65D**（纳芯微 NSSine™，
双核 Cortex-M7）上，功能与 [`../NS800RT7P65D/`](../NS800RT7P65D/README.md) 里的裸机版一一对应：

- 通过 **UART2** 接收 K230 视觉节点的 ASCII 行指令（`one` / `two` / `three` / `角度,Y`）
- 驱动 **总线舵机 ×3**（UART1，0x55 0x55 帧协议）、**步进电机**、**直流电机（吹风）**、**振动电机**
- 用 **ECAP1** 捕获超声波回波，完成测距
- 按原有时序完成作业并回 `workOK` / `noMode`

> ⚠️ **验证状态**：本目录代码是按 **RT-Thread 主线官方 BSP 源码**（`bsp/novosns/ns800/ns800rt7p65-nssinepad`）
> 逐项核对接口后编写的，**尚未在实物开发板上编译与运行验证**（开发板未到货）。
> 详见文末「[验证状态与待办](#验证状态与待办)」。

---

## 为什么需要这个版本

比赛要求「**指定主控 NS800RT7P65D 的 RT-Thread 版本**」。
原有裸机版（[`../NS800RT7P65D/`](../NS800RT7P65D/README.md)）直接调厂商 StdDriver + 主循环轮询，
不满足「RT-Thread」这条要求；本目录把同一套硬件驱动与业务逻辑移植到 RT-Thread。

两个版本**并存**：裸机版保留作为对照与备份，RT-Thread 版作为对外交付版本。

---

## 目录结构

```
NS800RT7P65D-RTThread/
├── README.md                       # 本文件
├── applications/                   # 直接覆盖到官方 BSP 的同名目录
│   ├── main.c                      #   应用入口（替换 BSP 自带的 main.c）
│   ├── app_config.h                #   引脚 / 串口 / 时序 / 协议参数集中配置
│   ├── app_servo.c/.h              #   总线舵机（uart 设备 + 互斥量）
│   ├── app_stepper.c/.h            #   步进电机（pin 设备 + rt_hw_us_delay）
│   ├── app_dcmotor.c/.h            #   直流电机（pwm 设备 EPWM1）
│   ├── app_vibration.c/.h          #   振动电机（pin 设备）
│   ├── app_ultrasonic.c/.h         #   超声波（ECAP1 单次捕获 + 信号量）
│   ├── app_control.c/.h            #   指令解析 + 动作编排（rx 线程 / 动作线程 / 消息队列）
│   └── app_msh.c                   #   msh 调试命令（逐模块测试 + 自检）
└── docs/
    ├── 01-移植对照表.md             #   裸机 API → RT-Thread API 对照
    ├── 02-接线与串口分配.md         #   完整接线表 + 串口冲突与两种方案
    └── 03-验收演示脚本.md           #   演示步骤与预期现象
```

---

## 一、软件基线

| 项目 | 内容 |
| --- | --- |
| RT-Thread | 主线版本，BSP 位于 `bsp/novosns/ns800/ns800rt7p65-nssinepad` |
| 获取方式 | `git clone https://gitee.com/rtthread/rt-thread.git`（GitHub 亦可） |
| 芯片支持包 | RT-Thread 的 `NOVOSNS Drivers` 软件包（`pkgs --update` 拉取，提供 `NS800RTxxxx.h` 等 StdDriver） |
| 开发板 | NS800RT7P65-NSSinePad 或自制板（引脚按 [02-接线与串口分配.md](docs/02-接线与串口分配.md) 连接） |
| 工具链 | ARM GCC（scons）或 Keil MDK5（`scons --target=mdk5` 生成工程） |

> BSP 默认只打开了 **GPIO + UART1（作控制台）**，其余外设需要在 menuconfig 里开。

---

## 二、集成步骤

### 1. 准备 RT-Thread 源码

```bash
git clone https://gitee.com/rtthread/rt-thread.git
cd rt-thread/bsp/novosns/ns800/ns800rt7p65-nssinepad
```

### 2. 用本目录的 `applications/` 覆盖 BSP 的 `applications/`

```bash
# 假设本仓库在第 4 级目录，按实际路径替换
rm -rf applications
cp -r <本仓库>/NS800RT7P65D-RTThread/applications ./
```

> `applications/SConscript` 里写的是 `Glob('*.c')`，所以只要把 `.c` 文件放进去就会被编译，
> 不需要改构建脚本。**注意必须删掉 BSP 原有的 `main.c`**，否则会和本目录的 `main.c` 重复定义。

### 3. 配置 menuconfig

```bash
scons --menuconfig     # 或 env 工具里执行 menuconfig
```

按下表勾选（`On-chip Peripheral Drivers` 分支下）：

| 选项 | 值 | 说明 |
| --- | --- | --- |
| `BSP_USING_UART1` | ✅（默认已开） | 总线舵机（PA12/PA13） |
| `BSP_USING_UART2` | ✅ **需手动打开** | 与 K230 通信（PB6/PB7） |
| `BSP_USING_UART3` | ✅ **需手动打开**（方案 A） | 控制台（PB24/PB25） |
| `BSP_USING_EPWM` | ✅ **需手动打开** | 会自动 `select RT_USING_PWM` |
| `BSP_USING_EPWM1` | ✅ **需手动打开** | 直流电机 PWM（PA0/PA1） |
| `BSP_USING_ECAP` | ❌ **保持关闭** | 超声波不用 BSP 的 eCAP 驱动，理由见下 |
| `BSP_USING_GPIO` | ✅（默认已开） | 步进 / 触发 / 振动 / NSLEEP |

**为什么关掉 ECAP？**
BSP 的 `drv_ecap.c` 固定为「连续捕获 + 4 个边沿 + 每个边沿复位计数器」，适合测连续 PWM；
而超声波每次触发只产生**一个**上升/下降沿，凑不齐 4 个事件，因此本应用直接在
`app_ultrasonic.c` 里把 ECAP1 配成单次捕获（与裸机版一致），两者不能同时启用 ECAP1。

### 4. 修改控制台串口

编辑 `ns800rt7p65-nssinepad/rtconfig.h`：

```c
/* 方案 A（默认）：控制台用 uart3，把 UART1 让给总线舵机 */
#define RT_CONSOLE_DEVICE_NAME "uart3"
```

> 若采用方案 B（控制台留在 `uart1`、舵机改接 UART3），则保持 `"uart1"` 不变，
> 并把 `applications/app_config.h` 里的 `APP_SERVO_ON_UART3` 改成 `1`。
> 两种方案的取舍见 [02-接线与串口分配.md](docs/02-接线与串口分配.md)。

### 5. 拉取芯片支持包并编译

```bash
pkgs --update           # 拉取 NOVOSNS 系列 StdDriver（必须，否则找不到 NS800RTxxxx.h）
scons -j8               # GCC 编译
# 或者生成 Keil 工程：
scons --target=mdk5
```

烧录后复位，控制台（UART3，115200 8N1）应看到：

```
============================================================
  花卉授粉机器人 · 执行端（RT-Thread 版）
  MCU : NS800RT7P65D (Cortex-M7, 双核)
  OS  : RT-Thread 5.x.x
  编译: <日期> <时间>
------------------------------------------------------------
  K230 指令串口 : uart2 @ 115200 8N1
  总线舵机串口  : uart1 @ 115200 8N1
  指令协议      : one / two / three 选模式，"角度,Y" 执行作业
============================================================
[I/app.servo] 舵机总线就绪: uart1 @ 115200
[I/app.step] 步进电机就绪: PUL/DIR/ENA = GPIO_2/GPIO_3/GPIO_4 (PA2/PA3/PA4)
[I/app.dc] 直流电机就绪: epwm1 A/B 通道, 20kHz
[I/app.vib] 振动电机就绪: GPIO_14 (PA14)
[I/app.us] 超声波就绪: TRIG=PA5, ECHO=PA16(ECAP1), 计数时钟 200 MHz
[I/app.ctrl] 业务层启动完成: K230 串口 uart2 @ 115200

[main] 系统就绪，等待 K230 下发指令...
msh >
```

---

## 三、软件架构

```
                     UART2 (PB6/PB7, 115200)
        K230 ─────────────────────────────────┐
                                              │ 接收中断
                                    ┌─────────▼─────────┐
                                    │ rx 线程 (prio 12) │  按行组装 + 解析
                                    └───┬──────────┬────┘
                        one/two/three   │          │  "角度,Y"
                        （直接生效）      │          ▼
                                        │   ┌──────────────┐
                                        │   │ 消息队列 ×8   │
                                        │   └──────┬───────┘
                                        │          │
                                        │   ┌──────▼─────────┐
                                        │   │ 动作线程(prio 15)│
                                        │   │ 步进→舵机→作业  │
                                        │   │ →回原点        │
                                        │   └──────┬─────────┘
                                        │          │ workOK
                                        ▼          ▼
                                    ┌──────────────────┐
                                    │  UART2 回复       │
                                    └──────────────────┘
```

**关键设计**

1. **模式指令与坐标指令走不同路径**：`one/two/three` 由 rx 线程直接改模式变量（插入临界区），
   **不必等当前动作做完**；坐标指令进消息队列排队，由动作线程串行执行。这与 K230 的
   「先发模式、再发坐标」的调用顺序吻合。
2. **动作线程里可以有阻塞延时**：`rt_thread_mdelay()` 让出 CPU，期间 rx 线程照常收指令，
   而裸机版是整机阻塞 3 秒。
3. **队列满时回复 `busy`**：避免 K230 等不到 `workOK` 而卡死（裸机版是直接丢弃）。
4. **模式在动作开始时取快照**：动作执行过程中改模式不会打乱当前这次作业。

---

## 四、与裸机版的对照

| 裸机版 | RT-Thread 版 | 说明 |
| --- | --- | --- |
| `Device_init()` / `Interrupt_initModule()` 等 | BSP 的 `rt_hw_board_init()` 自动完成 | 应用层不再调用 |
| `delay_init()` + `Delay_ms()` | `rt_thread_mdelay()` | 让出 CPU |
| `delay_us()` 忙等 | `rt_hw_us_delay()` | 保留忙等，用于脉冲时序 |
| `UART_writeChar()` 发舵机报文 | `rt_device_write()` + 互斥量 | 设备框架 |
| 主循环轮询 `Serial2_GetRxFlag()` | 接收中断 + 信号量 + rx 线程 | 事件驱动 |
| `while(1) Control_MainLoop()` | 动作线程 + 消息队列 | 线程化 |
| `PWM_SetCompare3/4()` 直写寄存器 | `rt_pwm_set()` / `rt_pwm_enable()` | PWM 设备框架 |
| `GPIO_setPin()` | `rt_pin_write()` | PIN 设备框架 |
| ECAP 直接配置 | **仍然直接配置** | BSP 的 eCAP 驱动不适用于单次回波 |

完整对照见 [docs/01-移植对照表.md](docs/01-移植对照表.md)。

---

## 五、调试命令（msh）

连上控制台后可直接输入：

| 命令 | 作用 | 示例 |
| --- | --- | --- |
| `mode` | 设定作业模式 | `mode one` |
| `act` | 投递一次坐标作业 | `act 30,120` |
| `dist` | 连续测距 | `dist 5` |
| `servo` | 单独驱动一路舵机 | `servo 1 350 1000` |
| `dc` | 直流电机占空比 | `dc 100` |
| `vib` | 振动指定毫秒 | `vib 1000` |
| `step` | 步进电机定位 | `step 45` |
| `status` | 打印整体状态 | `status` |
| `selftest` | 上电自检（依次驱动全部执行器） | `selftest` |
| `us_cal` | 测距校准系数 | `us_cal 0.99` |

---

## 六、常见问题

| 现象 | 原因与处理 |
| --- | --- |
| 编译报找不到 `NS800RTxxxx.h` | 没执行 `pkgs --update`，NOVOSNS StdDriver 包没拉下来 |
| 编译报 `RT_USING_PWM` 未定义 | menuconfig 里没开 `BSP_USING_EPWM`（它会 select `RT_USING_PWM`） |
| 控制台无输出 / 乱码 | 控制台串口在方案 A 下是 **UART3（PB24/PB25）**，不是板载 USB 口；波特率 115200 8N1 |
| 舵机不动 | ① 检查串口是否被控制台占用（见串口方案）；② 用 `servo 1 500` 单独测试；③ 舵机需外部供电，共地 |
| `epwm1` 找不到 | `BSP_USING_EPWM` / `BSP_USING_EPWM1` 没开 |
| 测距一直是 0 | ECHO 必须接 **PA16**；`dist 5` 看原始计数是否非 0；可用 `us_cal` 校准 |
| 动作队列满 | 上位机发太快；队列深度 8，可改 `APP_CMD_QUEUE_DEPTH` |

---

## 验证状态与待办

**已完成（可静态核对）**

- 逐项核对官方 BSP 源码后实现的设备名、引脚、API 签名：
  - `uart1/uart2/uart3` 设备名与引脚（`drv_uart.c` 的 `uart_config[]`）
  - `epwm1` 设备名、A/B 通道、period 单位为**微秒**（`drv_epwm.c` 的 `configuration_convert`）
  - `PIN_NUM(GPIO_xx)` 引脚编号方式（`drv_gpio.c` 的 `pin_map_table`，PA12=`GPIO_12`、PB6=`GPIO_38`）
  - `rt_pwm_set/rt_pwm_enable`、`rt_mq_init`、`rt_sem_*`、`rt_device_*` 的签名（RT-Thread 5.x 头文件）
- 指令协议、动作时序与裸机版逐条对齐（见 [docs/01-移植对照表.md](docs/01-移植对照表.md)）

**尚未验证（需实物）**

- 编译通过（本机无 ARM 工具链与 `pkgs --update` 后的 SDK）
- 各执行器实际动作、舵机行程数值、步进细分系数
- 超声波计数时钟与 58µs/cm 的换算精度（已提供 `us_cal` 现场校准）
- `epwm1` 的 period 单位若与本文档描述不符，需按实测调整 `APP_PWM_PERIOD_US`

**建议的Bring-up顺序**：`selftest` → `servo` → `step` → `dc` → `vib` → `dist` → 再接 K230 联调。
详见 [docs/03-验收演示脚本.md](docs/03-验收演示脚本.md)。
