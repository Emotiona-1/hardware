# 花卉授粉机器人 · 硬件代码合集

一套「**视觉识别 + 云端遥测 + 执行机构**」的机器人硬件代码，由三个独立节点组成：
K230 负责 AI 视觉识别，NS800RT7P65D 负责执行动作，ESP8266 负责环境温湿度上报。

> 本仓库是**分布式硬件代码合集**，三个节点各自独立运行、通过串口与 MQTT 协作。
> 每个子目录都有自己完整的 README。

---

## 系统组成

```
                 ┌──────────────────── 云端 / 上位机 ────────────────────┐
                 │  MQTT Broker（下发指令、接收温湿度）                   │
                 │  TCP 图像服务（接收病害抓拍 JPEG）                     │
                 └───────┬───────────────────────────────┬───────────────┘
                         │ MQTT                          │ TCP :8888
                 ┌───────▼────────┐              ┌───────▼────────────┐
                 │  ESP8266 节点   │              │  K230 视觉节点      │
                 │  DHT11 温湿度   │              │  AI 分类 + 抓拍     │
                 │  1 秒上报一次   │              │  HDMI/LCD 显示      │
                 └────────────────┘              └───┬────────────┬───┘
                                                     │ UART1      │ UART2
                                                     │ 115200     │ 9600
                                                     │ 动作指令    │ JSON 运动指令
                                             ┌───────▼──────┐   ┌─▼─────────────────┐
                                             │ NS800RT7P65D │   │ 运动/底盘控制器    │
                                             │ 执行端主控    │   │ （不在本仓库内）   │
                                             │ 舵机+电机     │   └───────────────────┘
                                             └──────────────┘
```

| 节点 | 目录 | 角色 | 与谁通信 |
| --- | --- | --- | --- |
| **K230 视觉节点** | [`K230/`](K230/README.md) | 摄像头采集 + AI 分类（花 / 果 / 病害），识别到病害时抓拍上传；接收 MQTT 指令转发给执行端 | MQTT、TCP、UART1→执行端、UART2→底盘 |
| **NS800RT7P65D 执行端** | [`NS800RT7P65D/`](NS800RT7P65D/README.md) | 收到动作指令后驱动步进电机、总线舵机、直流电机、振动电机完成作业动作 | UART2 ← K230 |
| **ESP8266 环境节点** | [`ESP8266/`](ESP8266/README.md) | DHT11 采集温湿度并通过 MQTT 上报；订阅下行消息打印到串口 | MQTT |

> 图中「运动/底盘控制器」只出现在 K230 的 UART2 发送目标里，**其代码不在本仓库中**。

---

## 目录结构

```
.
├── README.md                 # 本文件
├── LICENSE                   # MIT
├── .gitignore                # 已忽略 secrets.h / secrets.py 等本地配置
├── K230/                     # 视觉节点（MicroPython）
│   ├── README.md
│   ├── lastk230.py           #   主程序
│   ├── secrets.py.example    #   配置模板（复制为 secrets.py 后填写）
│   └── umqtt/simple.py       #   第三方 MQTT 客户端库
├── NS800RT7P65D/             # 执行端（裸机 C）
│   ├── README.md
│   ├── Hardware/             #   驱动层
│   └── User/                 #   main.c 与中断向量
└── ESP8266/                  # 环境节点（Arduino）
    ├── README.md
    ├── ESP8266_DHT11_MQTT.ino
    └── secrets.h.example     #   配置模板（复制为 secrets.h 后填写）
```

---

## 快速上手

### 1. K230 视觉节点

```bash
cd K230
cp secrets.py.example secrets.py     # 然后填入 WiFi、TCP 服务器、MQTT 信息
```

把 `lastk230.py`、`secrets.py`、`umqtt/` 一起放到 K230 的 `/sdcard/` 下，并把 AI 模型
放到 `/sdcard/the_model/`（**模型文件不在本仓库**），然后在 K230 的 MicroPython REPL 里执行
`lastk230.py`。详见 [K230/README.md](K230/README.md)。

### 2. NS800RT7P65D 执行端

把 `Hardware/` 与 `User/` 加入厂商 SDK 工程后编译烧录。**本仓库不含 SDK 与工程文件，
不能直接编译**，详见 [NS800RT7P65D/README.md](NS800RT7P65D/README.md)。

### 3. ESP8266 环境节点

```bash
cd ESP8266
cp secrets.h.example secrets.h       # 然后填入 WiFi、MQTT broker 与主题
```

用 Arduino IDE 打开 `ESP8266_DHT11_MQTT.ino` 编译烧录（注意：Arduino IDE 要求 `.ino`
所在文件夹与文件同名，目录名 `ESP8266` 与文件名不一致，需重命名文件夹或改用 PlatformIO）。
详见 [ESP8266/README.md](ESP8266/README.md)。

---

## 本地配置与密钥

**所有密码、服务器地址、MQTT broker 与主题都不在代码里**，而是通过两份被 `.gitignore`
忽略的本地文件注入：

| 文件 | 模板 | 被谁读取 |
| --- | --- | --- |
| `K230/secrets.py` | `K230/secrets.py.example` | `lastk230.py`（WiFi、TCP 服务器、MQTT 共 10 项配置） |
| `ESP8266/secrets.h` | `ESP8266/secrets.h.example` | `ESP8266_DHT11_MQTT.ino`（WiFi、MQTT、主题共 6 项配置） |

> ⚠️ **提交代码前请确认**：`secrets.py` / `secrets.h` 中是你的真实凭据，它们已被
> `.gitignore` 排除，请不要用 `git add -f` 强制添加。

---

## 硬件清单

| 部件 | 用途 | 备注 |
| --- | --- | --- |
| K230 开发板 | AI 视觉识别 | 需自备 MicroPython 固件与模型 |
| NS800RT7P65D 主控板 | 执行端主控 | 需自备厂商 SDK |
| ESP8266 模块 | 温湿度上报 | Arduino 平台 |
| DHT11 | 温湿度传感器 | 接 ESP8266 `D4`(GPIO2) |
| 超声波模块 | 测距（作业距离判定） | TRIG=PA5，ECHO=PA16 |
| 总线舵机 ×3 | 姿态 ×2、伸缩 ×1 | `0x55 0x55` 帧协议，115200 |
| 步进电机 + 驱动器 | 水平角度定位 | PUL/DIR/ENA 三线 |
| 直流电机 + 驱动 | 吹风 | PWM 双通道 + NSLEEP |
| 振动电机 | 振动 | GPIO 开关（可选 PWM 调速） |
| OLED 0.96" | 显示（预留） | 软件 I2C，PB14/PB15 |

精确到引脚的接线表见 [NS800RT7P65D/README.md](NS800RT7P65D/README.md#4-引脚分配表)。

---

## 本仓库不包含什么

- ❌ K230 的 **AI 模型文件**（`/sdcard/the_model/` 下的 `deploy_config.json` 与权重）
- ❌ NS800RT7P65D 的 **厂商 SDK、启动文件、链接脚本、IDE 工程**
- ❌ 任何编译产物、任何真实凭据（见上一节）
- ❌ K230 通过 UART2 所控制的**底盘/运动控制器**代码

---

## 许可证

本项目采用 [MIT 许可证](LICENSE)。

第三方代码：

- `K230/umqtt/simple.py` 来自 MicroPython 官方库 [micropython-lib](https://github.com/micropython/micropython-lib)，
  遵循 MIT 协议，版权归 MicroPython 项目所有。
- `NS800RT7P65D/Hardware/OLED_Font.h` 为流传广泛的 8×16 ASCII 点阵字库
  （常见于各类 OLED 教程），**未找到明确许可证声明**，如原作者有异议请提 Issue。

---

## 待作者确认

以下内容在代码中无法确定，欢迎作者补充：

1. NS800RT7P65D 的**厂商与 SDK 名称**、使用的 IDE 与烧录方式；
2. 总线舵机**型号**、直流电机**驱动芯片型号**；
3. 坐标指令中 `Y` 的**物理单位**，以及 `one`/`two`/`three` 三种作业模式的**正式名称**；
4. K230 的 UART2（9600）所连接的**底盘控制器**是哪个部件；
5. 三种作业模式分别对应的实际农艺动作（振动授粉 / 吹风 / 触摸？）。
