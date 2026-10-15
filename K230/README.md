# K230 视觉识别主程序（lastk230.py）

> 基于嘉楠 Canaan **K230** 与 **MicroPython + aicube/nncase** 的端侧视觉识别程序。
> 摄像头采集 → KPU 神经网络推理 → OSD 叠加显示；识别到目标后通过 **UART** 与主控 MCU 协同、
> 通过 **TCP** 上传病害抓拍、通过 **MQTT** 接收远程指令。

本目录是项目的 K230 子项目。协议为 **MIT**（见仓库根目录 `LICENSE`）。

---

## 目录

- [1. 功能概述](#1-功能概述)
- [2. 运行环境](#2-运行环境)
- [3. 目录结构](#3-目录结构)
- [4. 模型文件准备（重要）](#4-模型文件准备重要)
- [5. 配置文件 secrets.py 说明](#5-配置文件-secretspy-说明)
- [6. UART 通信协议](#6-uart-通信协议)
- [7. TCP 图像上传帧协议](#7-tcp-图像上传帧协议)
- [8. MQTT 指令协议表](#8-mqtt-指令协议表)
- [9. 主循环与容错流程](#9-主循环与容错流程)
- [10. 可调参数速查](#10-可调参数速查)
- [11. 常见问题（FAQ）](#11-常见问题faq)
- [12. 第三方代码与许可](#12-第三方代码与许可)

---

## 1. 功能概述

`lastk230.py` 是 K230 端的**唯一入口脚本**，运行时用一个 3 类目标检测模型对摄像头画面做实时推理，
把结果画在 HDMI/LCD 屏幕上，并与主控 MCU 分工协作。它是整个系统的「眼睛」，本身不驱动电机。

**识别的类别**（类别索引在代码中被显式定义为常量）：

| 常量 | 值 | 含义 | 代码中的用途 |
| --- | --- | --- | --- |
| `CLASS_FLOWER` | `0` | 花 | 计算中心点并映射成坐标，通过 UART1 发给主控（仅在"允许发送"状态下） |
| `CLASS_FRUIT` | `1` | 果 | 只做 OSD 显示，不触发任何外发动作 |
| `CLASS_DISEASE` | `2` | 病害 | 触发抓拍，把 JPEG 通过 TCP 上传到服务器 |

> 类别名称字符串来自模型配置 `deploy_config.json` 的 `categories` 字段，代码用
> `label = labels[class_id]` 取名字。代码注释里出现过 `tomato_disease` 字样，说明模型是
> **番茄（花 / 果 / 病害）三类检测模型**；实际标签以你自备的 `deploy_config.json` 为准。

**与主控 MCU 如何配合**（这是本程序最核心的协作逻辑）：

1. **上电握手（"f → o" 时序）**：程序完成初始化后，先通过 **UART2** 向云台/执行机构发
   `{"command":"forward"}`；等待 `F_O_INTERVAL_SEC = 3` 秒后再发 `{"command":"stop"}`，
   同时把 `flower_send_enabled` 置为 `True`。**只有在完成这个时序之后，花的坐标才允许外发。**
2. **单次触发**：一旦检测到花并且 `flower_send_enabled` 为 `True`，通过 **UART1** 发一次
   `<X>,<Y>\n` 坐标，并立刻把 `flower_send_enabled` 置回 `False`（**一帧只发一次**）。
3. **主控回执**：主控 MCU 处理完这轮动作后，通过 UART1 回一段包含字符串 `workOK` 的数据，
   K230 收到后重置状态并**重新执行整个 f→o 时序**，开始下一轮。如此循环往复。
4. **人工干预**：MQTT 订阅到的 `'1'` / `'2'` / `'3'` 会通过 UART1 发送 `one` / `two` / `three`
   各 4 次，供主控选择工作模式；`'f'/'b'/'l'/'r'/'s'` 则通过 UART2 直接驱动云台/底盘。
5. **病害上报**：检测到病害后，最快每 `DISEASE_SEND_INTERVAL_SEC = 10` 秒抓拍一张
   JPEG（质量 `JPEG_QUALITY = 40`）并通过 TCP 上传服务器，与主控动作互不阻塞。

**开机必须先等指令**：脚本启动后会阻塞在 `wait_for_mqtt_cmd()`，**必须先从 MQTT 收到
`'1'`/`'2'`/`'3'` 中的任意一条**，才会打开摄像头进入主循环。这是有意的设计（远程确认后再开跑），
调试时如果一直卡在 `Waiting for MQTT connection and boot command (1/2/3) ...`，说明指令没送到。

---

## 2. 运行环境

| 项目 | 要求 |
| --- | --- |
| 硬件 | 嘉楠 Canaan **K230** 开发板（如 K230D / 01Studio CanMV-K230 等），**需带 SD 卡** |
| 摄像头 / 显示 | CSI 摄像头；HDMI 屏（默认）或 ST7701 LCD 屏 |
| 固件 | **CanMV K230 MicroPython 固件**，需自带以下模块：`aicube`、`nncase_runtime`、`media.sensor`、`media.display`、`media.media`、`ulab.numpy`、`image`、`network`、`socket`、`machine.UART` / `machine.FPIOA` |
| 模型运行时 | `aicube` / `nncase_runtime`（随官方固件提供，**本仓库不包含**） |
| 模型文件 | 自备 `.kmodel` + `deploy_config.json`，放在板子 `/sdcard/the_model/`（**本仓库不包含**，见第 4 节） |
| MQTT 客户端 | 本仓库自带 `umqtt/simple.py`；若固件已内置 `umqtt.simple` 也可直接使用 |
| 网络 | 2.4 GHz WiFi（`network.STA_IF`）；一个 TCP 图像接收服务端；一个 MQTT broker |
| 主控 | 通过 UART 与 K230 相连的 MCU（本项目的执行端为 NS800RT7P65D，见 [`../NS800RT7P65D/`](../NS800RT7P65D/README.md)） |

> 脚本第 3–18 行的 import 列表即完整的运行依赖。**没有**用到 `urequests`、`ssl` 等额外模块。

---

## 3. 目录结构

仓库中大致的 K230 子项目布局：

```text
K230/
├── lastk230.py            # 主程序，唯一入口
├── secrets.py.example     # 本地凭据模板（入库）
├── secrets.py             # 本地凭据（【不入库】，已在 .gitignore 中忽略）
└── umqtt/
    └── simple.py          # MicroPython MQTT 客户端（第三方代码）
```

> 说明：仓库中的 `umqtt/` 目录**只有 `simple.py` 一个文件**（无 `__init__.py`），
> 模型目录 `the_model/` 及其内容**不在仓库中**，需按第 4 节自备。

烧录/拷贝到板子 SD 卡后的推荐布局（脚本内部路径是按这个布局写死的）：

```text
/sdcard/
├── lastk230.py            # 运行入口
├── secrets.py             # 必须与 lastk230.py 同目录（import secrets 才找得到）
├── umqtt/
│   └── simple.py          # 必须能被 import umqtt.simple 找到
└── the_model/             # ← 代码里的 root_path = "/sdcard/the_model/"
    ├── deploy_config.json # 模型配置文件（自备）
    └── xxxx.kmodel        # 模型权重，文件名由 deploy_config.json 的 kmodel_path 指定（自备）
```

代码中与路径相关的三行（已核对）：

```python
root_path   = "/sdcard/the_model/"                 # 模型根目录
config_path = root_path + "deploy_config.json"     # 即 /sdcard/the_model/deploy_config.json
kpu.load_kmodel(root_path + kmodel_name)           # 权重路径 = 根目录 + deploy_config 里的 kmodel_path
```

---

## 4. 模型文件准备（重要）

**本仓库不包含任何模型权重文件（`.kmodel`）和训练数据。** 你需要自行训练或获取模型，
并把它转换成 K230 可用的 kmodel 后放进板子的 `/sdcard/the_model/`。

程序启动时会读取 `/sdcard/the_model/deploy_config.json`，**以下字段缺一不可**
（键名与读取顺序均来自 `detection()` 中的 `deploy_conf[...]`）：

| 键名 | 代码中的用途 |
| --- | --- |
| `kmodel_path` | 权重文件名，实际加载路径 = `/sdcard/the_model/` + 该值 |
| `categories` | 类别名列表，`labels[class_id]` 用于 OSD 文字；**顺序必须与训练时一致**（0=花, 1=果, 2=病害） |
| `confidence_threshold` | 置信度阈值，传给 aicube 后处理 |
| `nms_threshold` | NMS 阈值 |
| `img_size` | 模型输入尺寸 `[w, h]`，同时用于 letterbox 计算 |
| `num_classes` | 类别数，应为 `3` |
| `nms_option` | NMS 方式（bool），传给 aicube 后处理 |
| `model_type` | `"AnchorBaseDet"` / `"GFLDet"` / 其它（走 `anchorfreedet_post_process`） |
| `anchors` | **仅当 `model_type == "AnchorBaseDet"` 时读取**，代码取 `anchors[0] + anchors[1] + anchors[2]` 拼成一维 |

后处理分支（代码原样）：

```python
if model_type == "AnchorBaseDet":
    det_boxes = aicube.anchorbasedet_post_process(..., anchors, nms_option)
elif model_type == "GFLDet":
    det_boxes = aicube.gfldet_post_process(..., nms_option)
else:
    det_boxes = aicube.anchorfreedet_post_process(..., nms_option)
```

> **注意**：`model_type` 写成 `"GFLDet"` / `"AnchorBaseDet"` 之外的值时，会静默走 anchor-free 分支，
> 不会报错。若发现检测框完全不对，优先检查这个字段。
> `anchors` 缺失时只有在 AnchorBaseDet 分支才会 `KeyError`。
>
> 输入预处理固定为：letterbox 等比缩放 + 灰边填充 `[114,114,114]`（`ai2d.set_pad_param`），
> 双线性插值半像素模式（`nn.interp_method.tf_bilinear` / `nn.interp_mode.half_pixel`），
> 输入张量格式 NCHW、`uint8`。这些是代码写死的，自备模型必须与之匹配。

---

## 5. 配置文件 secrets.py 说明

### 5.1 用法

仓库提供了模板 `secrets.py.example`，把它复制成 **同目录**下的 `secrets.py` 并填入自己的值。
`secrets.py` 已被仓库根目录 `.gitignore` 忽略，不会被提交。

```bash
cp secrets.py.example secrets.py   # 然后编辑 secrets.py
```

若忘记创建，脚本会在启动时直接抛出中文提示：

```text
ImportError: 缺少本地配置文件：请将 secrets.py.example 复制为 secrets.py 并填入 WiFi 与服务器信息
```

### 5.2 变量名逐个核对（**真实代码为准**）

以下对照基于 `lastk230.py` 第 41–59 行与 `secrets.py.example`：

| 变量名 | `lastk230.py` 是否从 `secrets` 导入？ | 代码中的用法 | 结论 |
| --- | --- | --- | --- |
| `WIFI_SSID` | ✅ 是（第 43 行） | `connect_wifi(WIFI_SSID, WIFI_PASS)` | **一致** |
| `WIFI_PASS` | ✅ 是（第 43 行） | 同上 | **一致** |
| `TCP_SERVER_IP` | ✅ 是（第 43 行） | `connect_tcp_server(TCP_SERVER_IP, TCP_SERVER_PORT)` | **一致** |
| `TCP_SERVER_PORT` | ✅ 是（第 43 行） | 同上 | **一致** |
| `MQTT_BROKER` | ✅ 是（第 44 行） | `socket.getaddrinfo(MQTT_BROKER, MQTT_PORT)` | **一致** |
| `MQTT_PORT` | ✅ 是（第 44 行） | 同上 | **一致** |
| `MQTT_TOPIC` | ✅ 是（第 44 行） | `mqtt_client.subscribe(MQTT_TOPIC)`：**订阅**指令主题 | **一致** |
| `MQTT_PUB_TOPIC` | ✅ 是（第 44 行） | `mqtt_client.publish(MQTT_PUB_TOPIC, b"OK")`：**发布**上线握手 | **一致** |
| `MQTT_USER` | ✅ 是（第 45 行） | `MQTTClient(..., user=MQTT_USER, ...)`，可为 `None` | **一致** |
| `MQTT_PASS` | ✅ 是（第 45 行） | 同上，可为 `None` | **一致** |

**结论**：10 个配置项**全部**从 `secrets.py` 导入，脚本内部不再保存 broker 地址、端口、
主题名或账号密码，仓库中也不含任何真实服务器地址。
**改配置只需改 `secrets.py`，不必动 `lastk230.py`。**

脚本内部只保留两个与部署环境无关的常量（不含任何地址或密码）：

| 常量 | 代码值 | 含义 |
| --- | --- | --- |
| `MQTT_CLIENT_ID` | `"k230_lastk230"` | 客户端 ID 前缀，连接时会追加 MAC 后 3 字节或随机数（见下） |
| `MQTT_KEEPALIVE` | `60` | keepalive 秒数，主循环每 `MQTT_KEEPALIVE // 2` = **30 秒** 发一次 `ping()` |

> 这两个常量与部署环境无关（既不含服务器地址也不含密码），因此直接写在脚本里。
> 如果希望连同它们一起外部化，也可以按同样方式加进 `secrets.py`。

### 5.3 secrets.py 字段含义

| 变量 | 说明 |
| --- | --- |
| `WIFI_SSID` | 2.4 GHz WiFi 名称 |
| `WIFI_PASS` | WiFi 密码 |
| `TCP_SERVER_IP` | JPEG 图像接收服务器地址。可以是域名（代码会先 `getaddrinfo` 打印解析结果）也可以是数字 IP |
| `TCP_SERVER_PORT` | TCP 服务端口，模板示例为 `8888` |
| `MQTT_BROKER` | MQTT broker 地址，模板占位 `broker.example.com`；DNS 不可用时可直接填数字 IP |
| `MQTT_PORT` | MQTT 端口，模板占位 `1883`（明文，未启用 TLS） |
| `MQTT_TOPIC` | **订阅**主题：接收下行指令（`1`/`2`/`3`/`s`/`l`/`r`/`f`/`b`），模板占位 `your_sub_topic` |
| `MQTT_PUB_TOPIC` | **发布**主题：上线握手 `OK`，模板占位 `your_pub_topic` |
| `MQTT_USER` | broker 用户名；不需要鉴权时填 `None` |
| `MQTT_PASS` | broker 密码；不需要鉴权时填 `None` |

> **主题方向提醒**：`MQTT_TOPIC` 是 K230 **订阅**的，`MQTT_PUB_TOPIC` 是 K230 **发布**的，
> 需与 ESP8266 端 `secrets.h` 的 `TOPIC_SUB` / `TOPIC_PUB` **交叉对应**（详见第 8 节）。
> 示例值请一律用文档占位符风格：`192.0.2.10`、`broker.example.com`。
> **切勿把真实域名 / 公网 IP / WiFi 密码提交到公开仓库。**

---

## 6. UART 通信协议

程序使用**两个 UART**，分工完全不同。引脚通过 `machine.FPIOA` 的 `set_function()` 复用，
并非默认引脚，**接线时必须以代码里的 GPIO 号为准**。

### 6.1 UART1 —— 与主控 MCU 的动作握手（115200）

| 项目 | 值（来自代码） |
| --- | --- |
| 实例 | `UART(UART.UART1, ...)` |
| 波特率 | **115200** |
| 数据位 / 校验 / 停止位 | `UART.EIGHTBITS` / `UART.PARITY_NONE` / `UART.STOPBITS_ONE`（8N1） |
| 引脚 | `fpioa.set_function(3, FPIOA.UART1_TXD)` → **GPIO3 = TX**；`fpioa.set_function(4, FPIOA.UART1_RXD)` → **GPIO4 = RX** |
| 方向 | **双向**：K230 发指令与坐标，主控回 `workOK` |
| 初始化失败时 | 打印 `UART init failed:`，`uart = None`，后续所有发送被跳过并打印 `WARNING: uart is None ...`（程序不会崩） |

**K230 → 主控（发送）**，共三类报文，全部是 **ASCII 文本行，以 `\n` 结尾**：

| 触发条件 | 发送内容 | 次数 | 说明 |
| --- | --- | --- | --- |
| 收到 MQTT `'1'` | `one\n` | **连续 4 次** | 同时置 `boot_cmd_received = True`，解除开机等待 |
| 收到 MQTT `'2'` | `two\n` | **连续 4 次** | 同上 |
| 收到 MQTT `'3'` | `three\n` | **连续 4 次** | 同上 |
| 识别到花（`class_id == 0`）且 `flower_send_enabled == True` | `<X>,<Y>\n` | 1 次 | 形如 `"-42,133\n"`，纯十进制整数，逗号分隔；发送后立即把 `flower_send_enabled` 置 `False` |

坐标映射公式（代码原样，目标是把推理图坐标换算成给主控的角度/位置量）：

```python
mapped_x = int(-(x_center / OUT_RGB888P_WIDTH  * 180 - 90))   # x_center 为检测框中心，结果落在 [-90, 90]
mapped_y = int( y_center / OUT_RGB888P_HEIGHT * 200)          # 结果落在 [0, 200)
```

> `OUT_RGB888P_WIDTH = ALIGN_UP(1080, 16)`（16 字节对齐后为 **1088**）、`OUT_RGB888P_HEIGHT = 720`，
> 即推理/坐标基准是 **1088×720** 的 RGB888P 通道。
> 这两个数值的物理含义（角度？像素？步数？）代码中没有注释，**需作者确认**。

**主控 → K230（接收）**：主循环里用 `uart.any()` + `uart.read()` 轮询，**只有当读到的数据中包含
子串 `workOK` 时**才处理（`if rx_data and b"workOK" in rx_data:`），其余内容一律丢弃。
收到后的动作：

1. `flower_send_enabled = False`（先禁止发花坐标）
2. 通过 UART2 发 `{"command":"forward"}`
3. `f_o_phase = "WAIT_AFTER_F"`，记 `f_sent_time = utime.time()`，重启 f→o 时序

> ⚠️ 该分支按「整包包含子串」判断，没有帧头/校验，也没有超时清缓冲；如果主控发的数据里恰好
> 混入 `workOK` 会误触发。是否需要更严格的帧格式，**需作者确认**。

### 6.2 UART2 —— 云台 / 底盘运动指令（9600）

| 项目 | 值（来自代码） |
| --- | --- |
| 实例 | `UART(UART.UART2, ...)` |
| 波特率 | **9600**（常量 `UART2_BAUDRATE = 9600`） |
| 数据位 / 校验 / 停止位 | 8N1（同上） |
| 引脚 | `fpioa.set_function(11, FPIOA.UART2_TXD)` → **IO11 = TX2**；`fpioa.set_function(12, FPIOA.UART2_RXD)` → **IO12 = RX2** |
| 方向 | 代码中**只发不收**：RX2 被配置但从未 `read()` |
| 报文格式 | JSON 文本，**不带换行符**（`uart2.write(json_str)` 直接写字符串） |

**UART2 发出的全部报文集**（代码里出现的字符串，逐条来自源码）：

| JSON 报文 | 触发来源 |
| --- | --- |
| `{"command":"forward"}` | ① 开机时序起始；② 收到 UART1 `workOK` 后重启时序；③ MQTT `'f'` |
| `{"command":"stop"}` | ① 开机时序等待 `F_O_INTERVAL_SEC = 3` 秒后；② MQTT `'s'` |
| `{"command":"left"}` | MQTT `'l'` |
| `{"command":"right"}` | MQTT `'r'` |
| `{"command":"backward"}` | MQTT `'b'` |

> 源码第 425 行注释明确写着「MQTT 接收取代 UART2 接收」，即**下行指令通道已从 UART2 迁到 MQTT**，
> UART2 现在只作为单向控制输出。UART2 的 RX2（IO12）目前是预留，**需作者确认**后续是否启用。

### 6.3 两个 UART 的分工小结

| | UART1 | UART2 |
| --- | --- | --- |
| 用途 | 与主控 MCU 的**模式指令 + 花坐标 + workOK 握手** | 云台/底盘**运动 JSON 指令** |
| 波特率 | 115200 | 9600 |
| 引脚 | GPIO3 (TX) / GPIO4 (RX) | IO11 (TX2) / IO12 (RX2) |
| 报文风格 | `one` / `two` / `three` / `X,Y` + `\n` | `{"command":"..."}` 无换行 |
| 收发 | 双向 | 只发 |

---

## 7. TCP 图像上传帧协议

病害图片走 TCP，帧头帧尾由脚本顶部两个常量定义：

```python
FRAME_HEADER = b'\xAA\xBB\xCC\xDD'
FRAME_FOOTER = b'\xEE\xFF\x00\xFF'
```

**实际用途（重要澄清）**：这两个常量**只用在 TCP 图像包上，没有用在任何 UART 通信里**。
`send_tcp_image()` 中的组包逻辑（代码原样）：

```python
packet = FRAME_HEADER + len(jpeg_data).to_bytes(4, 'big') + jpeg_data + FRAME_FOOTER
sock.sendall(packet)
```

因此一帧 TCP 报文的字节布局为：

| 偏移 | 长度 | 内容 |
| --- | --- | --- |
| 0 | 4 B | 帧头 `AA BB CC DD` |
| 4 | 4 B | **JPEG 数据长度，大端（big-endian）uint32** |
| 8 | N B | JPEG 原始字节（`image.to_jpeg(quality=40)` 输出，以 `FF D8` 开头） |
| 8+N | 4 B | 帧尾 `EE FF 00 FF` |

其它细节：

- 发送前会校验 JPEG 魔数 `0xFF 0xD8`，不合法则打印告警并**放弃本次发送**（返回 0）。
- 抓拍用的是 `sensor.snapshot(chn=CAM_CHN_ID_0)`（YUV420 通道），与推理用的 `CAM_CHN_ID_2` 分开，
  因此上传的图是**原始全画幅**，不是 letterbox 后的推理输入。
- 服务器地址就是 `secrets.py` 里的 `TCP_SERVER_IP:TCP_SERVER_PORT`；连接超时 5 秒
  （`settimeout(5)`），连上后切回阻塞模式（`setblocking(True)`）以保证 `sendall` 完整发出。
- **本仓库不含服务端**，接收端需自备（按上表解析即可；因为长度字段已给出，读满 N 字节再校验帧尾最稳妥）。
- 发送失败时会 `close()` 并把 `tcp_socket = None`，随后在 WiFi 检查分支里自动重连。

---

## 8. MQTT 指令协议表

`mqtt_cmd_callback(topic, msg)` 是全量指令入口。**报文会先 `msg.strip()` 去掉 `\r` `\n` 等空白**，
所以 `"1\n"` 与 `"1"` 等价；匹配是**严格的字节相等**，多一个空格就会落到「忽略」分支。

| 收到内容（strip 后） | 行为 | 作用的 UART | 是否解除开机等待 |
| --- | --- | --- | --- |
| `b"1"` | UART1 连发 **4 次** `one\n` | UART1 @115200 | ✅ `boot_cmd_received = True` |
| `b"2"` | UART1 连发 **4 次** `two\n` | UART1 @115200 | ✅ |
| `b"3"` | UART1 连发 **4 次** `three\n` | UART1 @115200 | ✅ |
| `b"s"` | UART2 发 `{"command":"stop"}` | UART2 @9600 | ❌ |
| `b"l"` | UART2 发 `{"command":"left"}` | UART2 @9600 | ❌ |
| `b"r"` | UART2 发 `{"command":"right"}` | UART2 @9600 | ❌ |
| `b"f"` | UART2 发 `{"command":"forward"}` | UART2 @9600 | ❌ |
| `b"b"` | UART2 发 `{"command":"backward"}` | UART2 @9600 | ❌ |
| 空消息 `b""` | **静默忽略**（`elif cmd != b""` 不成立） | — | ❌ |
| 其它任意内容 | 打印 `MQTT rx ignored (noise?):` + `msg.hex()`，**不执行任何动作** | — | ❌ |
| 回调内部异常 | 捕获后打印 `MQTT callback error:`，不影响主循环 | — | ❌ |

**MQTT 连接参数（地址与主题全部来自 `secrets.py`）**：

| 参数 | 值 |
| --- | --- |
| broker | `secrets.py` → `MQTT_BROKER`（模板占位 `broker.example.com`） |
| 端口 | `secrets.py` → `MQTT_PORT`（模板占位 `1883`，明文 MQTT，**未启用 TLS**） |
| 订阅主题 `MQTT_TOPIC` | `secrets.py` → `MQTT_TOPIC`（模板占位 `your_sub_topic`） |
| 发布主题 `MQTT_PUB_TOPIC` | `secrets.py` → `MQTT_PUB_TOPIC`（模板占位 `your_pub_topic`） |
| 用户名 / 密码 | `secrets.py` → `MQTT_USER` / `MQTT_PASS`（模板为 `None`，匿名） |
| keepalive | `60` 秒（脚本内常量 `MQTT_KEEPALIVE`） |
| client id | `"k230_lastk230"` + 后缀（脚本内常量 `MQTT_CLIENT_ID`） |
| 上线握手 | 连接成功后向 `MQTT_PUB_TOPIC` 发布 `b"OK"` |

**Client ID 去重逻辑**（避免多台设备互相踢下线）：首次连接时把 `MQTT_CLIENT_ID` 拼上
**WiFi MAC 地址后 3 字节的十六进制**（`ubinascii.hexlify(mac[-3:])`）；若取 MAC 失败则退化为
`"k230_lastk230_" + 随机数(1000~99999)`。拼接只做一次，之后不再变化。

**订阅是在 `connect()` 之后立刻执行的**，且 `set_callback()` 在 `connect()` 之前调用
（`umqtt.simple` 的 `subscribe()` 内部 `assert self.cb is not None`，顺序不能颠倒）。

---

## 9. 主循环与容错流程

### 9.1 启动时序

```text
detection() 开始
  ├─ 读取 /sdcard/the_model/deploy_config.json（ujson.load）
  ├─ 初始化 KPU / ai2d（letterbox 参数、NCHW uint8、双线性半像素）
  ├─ 加载 kmodel：kpu.load_kmodel("/sdcard/the_model/" + kmodel_path)
  ├─ 初始化 UART1 (115200, GPIO3/4)   ── 失败仅告警，置 None 继续
  ├─ 初始化 UART2 (9600,  IO11/12)    ── 失败仅告警，置 None 继续
  ├─ connect_wifi()  超时 20 秒（20 次 0.5s 轮询）
  │     └─ 成功：getaddrinfo(TCP_SERVER_IP) 打印解析结果
  │              → connect_tcp_server() → connect_mqtt()
  │        失败：打印 "WiFi not connected, skip TCP/MQTT init"
  ├─ wait_for_mqtt_cmd()   ★ 阻塞等待 MQTT 下发 '1'/'2'/'3'
  ├─ Sensor/Display/MediaManager 初始化（HDMI: LT9611 或 LCD: ST7701）
  ├─ MediaManager.init() + sensor.run()
  ├─ UART2 发 {"command":"forward"}，f_o_phase = "WAIT_AFTER_F"
  └─ 进入 while True 主循环
```

**`wait_for_mqtt_cmd()` 的容错**（开机等待期间同样具备自愈能力）：

- 每轮先看 WiFi：`wlan is None or not wlan.isconnected()` → **每 3 秒**重连一次 WiFi，
  成功后立刻补做 `connect_tcp_server()` + `connect_mqtt()`
- WiFi 正常但 `mqtt_client is None` → 同样**每 3 秒**重试 `connect_mqtt()`
- 有客户端时调 `mqtt_client.check_msg()`；**check_msg 抛异常就 disconnect 并置 None**，交给上面重连
- 每轮调 `mqtt_keepalive()`；循环末尾 `utime.sleep(0.05)`
- 只有回调把 `boot_cmd_received` 置 `True` 才退出循环，打印 `MQTT command received, continue boot.`

### 9.2 主循环每帧做的事（顺序即代码顺序）

| 顺序 | 逻辑 | 触发条件 / 周期 | 失败处理 |
| --- | --- | --- | --- |
| 1 | `sensor.snapshot(chn=CAM_CHN_ID_2)` 抓推理帧 | 每帧 | 整帧被 try/except 包住，异常打印 `Single frame error skip:` 后跳过该帧继续 |
| 2 | **WiFi 健康检查** | `frame_start - last_wifi_check >= 10` 即每 **10 秒** | 掉线 → 重连 WiFi → 成功后补 `connect_tcp_server()` + `connect_mqtt()`；在线但 `mqtt_client is None` → 只重连 MQTT |
| 3 | **UART1 收包**：`uart.any()` → `uart.read()`，命中 `b"workOK"` 才处理 | 有数据时 | 读失败打印 `UART read failed:` |
| 4 | `mqtt_client.check_msg()` 收取下行指令 | 每帧 | 异常 → disconnect + `mqtt_client = None`，**依赖第 2 步的 10 秒检查自动重连** |
| 5 | `mqtt_keepalive()` | 距上次 ping ≥ `MQTT_KEEPALIVE // 2` = **30 秒** | `ping()` 失败打印 `MQTT ping failed, connection dead:` → disconnect + 置 None |
| 6 | **f→o 时序收尾** | `f_o_phase == "WAIT_AFTER_F"` 且距 `f_sent_time` ≥ `F_O_INTERVAL_SEC = 3` 秒 | UART2 发 `{"command":"stop"}`，`flower_send_enabled = True`，`f_o_phase = "IDLE"` |
| 7 | 推理：`to_numpy_ref()` → `ai2d_builder.run()` → `kpu.set_input_tensor(0, ...)` → `kpu.run()` | 帧格式为 `image.RGBP888` 时 | — |
| 8 | 取出全部输出张量并 `reshape` 成一维，交给 aicube 后处理得到 `det_boxes` | 每帧 | 后处理分支按 `model_type` 选择 |
| 9 | 遍历检测框：OSD 画框 + `draw_string_advanced` 写字；`flower` 触发 UART1 坐标；`disease` 置 `disease_detected_in_frame = True` | 有检测框时 | UART 写失败打印 `UART write failed:` |
| 10 | **病害上报** | `disease_detected_in_frame` 且距 `last_disease_send_time` ≥ `DISEASE_SEND_INTERVAL_SEC = 10` 秒 | 发送异常 → close 并把 `tcp_socket = None`，若 WiFi 在线立即 `connect_tcp_server()` 重连 |
| 11 | `Display.show_image(osd_img, 0, 0, Display.LAYER_OSD3)` 刷新 OSD | 每帧 | — |
| 12 | `del` 临时引用 + `gc.collect()`（多处主动回收：帧末、输出张量后、JPEG 发送后） | 每帧 | — |

### 9.3 f→o 时序状态机（`f_o_phase`）

```text
   (开机 / 收到 workOK)
          │
          ▼
  ┌──────────────────┐   UART2: {"command":"forward"}      时间到（3s）
  │ WAIT_AFTER_F     │ ────────────────────────────────────────────────┐
  └──────────────────┘                                                 │
          ▲                                                            ▼
          │                                            UART2: {"command":"stop"}
          │  UART1 收到 "workOK"（重新计时，可打断 IDLE）   flower_send_enabled = True
          │                                                            │
          └──────────────────────── IDLE ◄─────────────────────────────┘
                                     │
                    检测到花 → UART1 发 "X,Y\n"
                    flower_send_enabled = False（等待下一次 workOK 才重新放行）
```

> 状态名里的 `f` / `o` 在代码注释中对应「上电后须完成 f→o 循环才置 True」，
> 实际动作是 **forward → (3 秒) → stop**。字母的确切含义（是否对应某两个旧指令）
> 代码中无从判断，**需作者确认**。

### 9.4 退出清理

主循环是 `while True`，正常情况下不会退出。若因外部原因跳出，收尾块（`try/except` 包裹，
**任何异常都被静默吞掉**）会依次执行：`os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)` → 释放张量 →
`sensor.stop()` → `Display.deinit()` → `MediaManager.deinit()` → 关闭 TCP → 断开 MQTT →
断开并停用 WLAN → `gc.collect()` → `time.sleep(1)` → `nn.shrink_memory_pool()`。

---

## 10. 可调参数速查

| 常量 / 变量 | 代码值 | 位置 | 说明 |
| --- | --- | --- | --- |
| `display_mode` | `"hdmi"` | 第 27 行 | 改 `"lcd"` 切到 ST7701 + 800×480 |
| `DISPLAY_WIDTH/HEIGHT` | HDMI: `ALIGN_UP(1920,16)` / `1080`；LCD: `ALIGN_UP(800,16)` / `480` | 第 28–33 行 | 显示与 OSD 分辨率 |
| `OUT_RGB888P_WIDTH/HEIGHT` | `ALIGN_UP(1080,16)`（=1088）/ `720` | 第 35–36 行 | 推理与坐标基准 |
| `JPEG_QUALITY` | `40` | 第 47 行 | 上传 JPEG 质量 |
| `DISEASE_SEND_INTERVAL_SEC` | `10` | 第 48 行 | 病害抓拍最小间隔（秒） |
| `UART2_BAUDRATE` | `9600` | 第 50 行 | UART2 波特率 |
| `F_O_INTERVAL_SEC` | `3` | 第 51 行 | forward→stop 间隔（秒） |
| `debug_mode` | `0` | 第 91 行 | **改为 1 可打印每帧推理耗时**（`ScopedTiming`） |
| `MQTT_KEEPALIVE` | `60` | 第 60 行 | keepalive 秒；ping 周期为其一半 |

`ScopedTiming` 会在 `__exit__` 打印形如 `total took 12.34 ms` 的耗时；`debug_mode = 0` 时整体静默。

---

## 11. 常见问题（FAQ）

### Q1. 一直提示 `MQTT DNS resolve failed:` / `TCP DNS resolve failed:`

K230 在部分网络下**拿不到可用 DNS**。解决办法：在 `secrets.py` 里把 `MQTT_BROKER`（以及
`TCP_SERVER_IP`）**改成数字 IP**，例如 `192.0.2.10`（在自己的云主机上跑一个
mosquitto / EMQX broker）。所有地址都集中在 `secrets.py`，不需要改 `lastk230.py`
（见第 5 节）。此外可用的排查手段：先用 `wlan.ifconfig()` 确认拿到了 IP 和网关，
再用 `socket.getaddrinfo()` 单独测一次解析。

> 顺带一提：`connect_mqtt()` 内部会先做一次 DNS 并把结果传给 `MQTTClient`，
> 而 `umqtt.simple` 在 `connect()` 里又会**自己再解析一次**（`socket.getaddrinfo(self.server, self.port)`）。
> 所以 DNS 不可用时即使第一处绕过了也没用。

### Q2. 启动打印 `WARNING: umqtt.simple not found, MQTT will be disabled`

说明 `import` 失败，`MQTT_AVAILABLE = False`，此后 `connect_mqtt()` 直接返回 False，
`wait_for_mqtt_cmd()` 会**永久阻塞在等待开机指令**（因为它依赖 MQTT 回调）。
排查方法：

1. 确认 `umqtt/simple.py` 已随脚本一起拷到板子上的 `/sdcard/umqtt/`，**目录名必须是 `umqtt`**。
   本仓库中的 `umqtt/` 目录里**只有 `simple.py`，没有 `__init__.py`**（作者环境下可正常 `import`）。
   如果你所用的 MicroPython 版本要求包目录显式存在 `__init__.py`，请补一个空文件再试 ——
   该固件是否必须 `__init__.py`，**需作者确认**。
2. 确认板子上的 Python 工作目录就是脚本所在目录（`import umqtt.simple` 依赖 `sys.path`）。
3. 该模块是第三方代码，本仓库随附一份（见第 12 节），可直接使用；若固件已内置则无需再拷。

### Q3. 内存不足 / `MemoryError` / 跑一会儿就重启

K230 上 KPU、ai2d、JPEG 编码都很吃内存，脚本已做多处优化，遇到问题可依次尝试：

- 确认代码里的 `gc.collect()` 调用**没有被删掉**（帧末、取完输出张量、发完 JPEG 各一次）。
- 降低分辨率：`OUT_RGB888P_WIDTH/HEIGHT` 或 `DISPLAY_*` 调小；`display_mode = "lcd"` 也比 HDMI 省。
- 降 `JPEG_QUALITY`（越小越省）。
- 缩短客户端 ID、减少同时持有的 bytes（`packet` 是一次性拼接的，大图时峰值内存 ≈ JPEG 的 2 倍）。
- 退出时调用的 `nn.shrink_memory_pool()` 只在收尾执行，**运行中不会自动释放 KPU 内存池**。

### Q4. 屏幕全黑 / 没有 OSD

`display_mode` 与实际屏幕必须匹配：HDMI 用 `Display.LT9611`，LCD 用 `Display.ST7701`。
LCD 模式下如果 `DISPLAY_WIDTH/HEIGHT` 与屏幕不符也会黑屏。可先改 `debug_mode = 1` 看每帧耗时
是否在推进，以区分「没画面」还是「没推理」。

### Q5. 检测框位置/类别不对

按顺序检查：① `deploy_config.json` 的 `categories` 顺序是否为「花/果/病害」；
② `model_type` 是否拼写正确（拼错会静默走 anchor-free 分支）；
③ `img_size` / `confidence_threshold` / `nms_threshold` 是否与训练导出时一致；
④ `model_type == "AnchorBaseDet"` 时 `anchors` 是否为 3 组（代码要取 `[0]+[1]+[2]`）。

### Q6. 花坐标只发一次就不再发

**这是设计行为**：发一次后 `flower_send_enabled` 被置 `False`，必须等主控通过 UART1 回
`workOK` 才会重新走 f→o 时序放行。如果主控没回 `workOK`，K230 不会再发任何坐标。
排查主控是否真的发出了包含 `workOK` 的数据、波特率是否为 **115200**、TX/RX 是否接反
（K230 GPIO3=TX ↔ 主控 RX，GPIO4=RX ↔ 主控 TX），以及是否共地。

### Q7. 病害图片上传不了

① 确认 `TCP_SERVER_IP/PORT` 可达（先 `ping` 或 `nc`）；② 服务端是否按第 7 节的帧格式解析；
③ 看日志里是 `TCP connect failed:` 还是 `TCP send failed:` / `Invalid JPEG header!`；
④ TCP 只会在**检出病害**且距上次 ≥10 秒时才发，静置时长期没有流量是正常的。

### Q8. 改了 secrets.py 里的 MQTT 配置却没生效

先确认改的是**同目录**下的 `secrets.py`（不是 `secrets.py.example`），并且新文件确实已经
上传到 K230；`MQTT_*` 六项都是从 `secrets.py` 导入的（见第 5.2 节）。若仍不生效，
检查是否残留旧的 `secrets.pyc` 缓存，或程序上电后没有重新运行 `lastk230.py`。

### Q9. 无法连接 broker：`MQTT connect failed:`

按顺序排查：① broker 地址与端口（1883 明文，**未启用 TLS**，不要用 8883）；
② 该 broker 是否允许匿名（当前 `MQTT_USER`/`MQTT_PASS` 均为 `None`，需要鉴权的 broker 会拒绝）；
③ 客户端 ID 冲突——代码已自动追加 MAC 后缀来规避，但若你手动改成固定 ID，多台设备会互相踢下线；
④ `MQTT connect failed:` 后面跟的是 `repr(e)`，`MQTTException(5)` 类返回值可对照 MQTT 返回码表。

---

## 12. 第三方代码与许可

- 本子项目代码以 **MIT** 协议发布，见仓库根目录 [`LICENSE`](../LICENSE)。
  （本文按「README 位于子项目目录 `K230/`、`LICENSE` 与 `.gitignore` 位于其上一级仓库根目录」的相对位置书写，
  若你的目录层级不同，请相应调整链接。）
- `umqtt/simple.py` 为 **MicroPython 官方 `micropython-lib` 的 `umqtt.simple`**（MIT 协议），
  属第三方代码，版权归原作者所有，请保留其许可声明。
- 需要说明的是，本仓库中的这份 `simple.py` 相比常见上游版本**增加了读超时逻辑**：
  构造函数里的 `self._read_timeout = 10`，以及 `_read()` 中基于 `utime.time()` 的 deadline 重试
  （超时抛 `MQTTException("read timeout")`）。是否为本地改动**需作者确认**，
  发布前建议在文件头补上上游来源与修改说明。
- `aicube` / `nncase_runtime` / `media.*` 等模块由 Canaan 官方 K230 MicroPython 固件提供，
  **不随本仓库分发**。
- 模型权重（`.kmodel`）与训练产物**不在本仓库中**，其许可由模型作者自行决定。
