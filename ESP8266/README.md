# ESP8266 + DHT11 温湿度 MQTT 上报节点

> 一个极简的 Arduino ESP8266 草图：**DHT11 采集温湿度 → 周期性地以 JSON 发布到 MQTT**，
> 同时**订阅一个下行主题，把收到的内容原样打印到串口**。
> 在本项目里它作为环境数据节点，与 K230 视觉节点共用同一个 MQTT broker。

协议为 **MIT**（见仓库根目录 `LICENSE`）。单文件草图，无云端依赖。

---

## 目录

- [1. 功能概述](#1-功能概述)
- [2. 依赖库](#2-依赖库)
- [3. 接线表](#3-接线表)
- [4. secrets.h 配置说明](#4-secretsh-配置说明)
- [5. MQTT 主题与报文格式](#5-mqtt-主题与报文格式)
- [6. 上报周期与运行流程](#6-上报周期与运行流程)
- [7. 编译与烧录](#7-编译与烧录)
- [8. 与 K230 主题配对的注意事项](#8-与-k230-主题配对的注意事项)
- [9. 常见问题（FAQ）](#9-常见问题faq)
- [10. 许可](#10-许可)

---

## 1. 功能概述

整个草图只有 3 个业务动作，非常容易移植：

1. **联网**：以 `WIFI_STA` 模式连接 `WIFI_SSID` / `WIFI_PASSWORD`，阻塞等待直到连上为止。
2. **上行**：每 `INTERVAL`（**1000 ms = 1 秒**）读一次 DHT11，读取成功就把温湿度拼成
   **紧凑 JSON** 发布到 `TOPIC_PUB`；读取失败则**什么都不发**（不报错、不补发）。
3. **下行**：`PubSubClient` 收到 `TOPIC_SUB` 的消息时，把 payload **原样打印到串口**，
   不做解析、不打印主题名、不做任何控制动作。

串口（USB，**115200**）同时是这个节点的调试口和下行数据的呈现口 —— 也就是说，
你可以把 MQTT 当成一个「隔空打串口」的通道来用。

> 注意：这个节点**只上报温湿度，不向 K230 发送任何指令**。详见第 8 节。

---

## 2. 依赖库

以下 `#include` 直接来自草图第 10–19 行：

| 头文件 | 类型 | 说明 |
| --- | --- | --- |
| `ESP8266WiFi.h` | 开发板核心自带 | 随 **ESP8266 Arduino Core**（esp8266/Arduino）安装，无需单独装库 |
| `PubSubClient.h` | 第三方库 | MQTT 客户端。API（`setServer` / `setCallback` / `connect` / `subscribe` / `publish` / `loop`）与 **Nick O'Leary 的 PubSubClient** 一致；版本**需作者确认** |
| `DHT11.h` | 第三方库 | DHT11 驱动，代码用到 `DHT11 dht11(pin)` 与 `dht11.readTemperatureHumidity(t, h)`（返回 `0` 表示成功）。**具体库名/作者/版本需作者确认**（Arduino 库管理器里有多个同名 `DHT11` 库，API 不完全相同） |
| `"secrets.h"` | 本地文件 | 用**双引号**包含，即**同目录**下的本地配置；仓库提供 `secrets.h.example` 模板，`secrets.h` 已被 `.gitignore` 忽略 |

> PlatformIO 用户在 `lib_deps` 里按实际所用库填写；PubSubClient 通常写作 `knolleary/PubSubClient`，
> DHT11 库**需作者确认**后再填。

---

## 3. 接线表

### 3.1 DHT11

引脚来自草图头部注释（第 4–7 行）与第 26 行的 `const int DHT_PIN = D4;`：

| DHT11 引脚 | 接到 ESP8266 | 说明 |
| --- | --- | --- |
| VCC | **3.3V** | 注释原文：`DHT11 VCC → 3.3V` |
| GND | **GND** | 注释原文：`DHT11 GND → GND` |
| DAT（DATA） | **D4** | 注释原文：`DHT11 DAT → D4 (GPIO2)`；代码中 `DHT_PIN = D4`，即 **GPIO2** |

> - 这里用的是 Arduino 的 **D4 丝印编号**，对应 ESP8266 的 **GPIO2**（NodeMCU / Wemos D1 mini 系列）。
>   换板子时 `D4` 的物理位置会变，`DHT_PIN` 需要相应修改。
> - 代码注释只列了 **3 个引脚**（VCC/GND/DAT），即按 **3 脚 DHT11 模块**接线。
>   裸传感器是 4 脚（其中一个为空脚 NC），另外**通常需要在 DAT 与 VCC 之间加 4.7 kΩ–10 kΩ 上拉电阻**
>   （DHT11 模块一般已板上集成）—— 这部分属于硬件常识，**代码中未体现，需作者确认实际硬件形态**。
> - ⚠️ DHT11 供电用 **3.3V**：DHT11 可工作在 3.3V，但若用 5V 供电，DAT 上的 5V 电平可能损伤
>   ESP8266 的 IO，**不要**把 DAT 直连 5V 逻辑。

### 3.2 串口

| 项目 | 值 | 来源 |
| --- | --- | --- |
| 波特率 | **115200** | `Serial.begin(115200)` |
| 用途 | 调试日志（WiFi/MQTT 状态）+ 下行 MQTT 报文原样输出 | `Serial.print` / `Serial.println` |
| 引脚 | 由开发板 USB-UART 芯片决定（NodeMCU：TX/RX 固定），**代码未做任何 Serial 重映射** | — |

---

## 4. secrets.h 配置说明

### 4.1 用法

仓库提供 `secrets.h.example` 模板，复制为**同目录**下的 `secrets.h` 并填入自己的值：

```bash
cp secrets.h.example secrets.h   # 然后编辑 secrets.h
```

草图第 15–19 行的注释说明了约定：**WiFi 账号密码、MQTT 服务器地址与主题都放在 `secrets.h`
（已加入 `.gitignore`）**。请勿把真实值写进 `.example` 模板。

### 4.2 字段逐个核对（以 `secrets.h.example` 与草图为准）

| 宏 | 模板中是否存在 | 代码中使用位置 | 说明 |
| --- | --- | --- | --- |
| `WIFI_SSID` | ✅ | `WiFi.begin(WIFI_SSID, WIFI_PASSWORD)` | WiFi 名称（**必须 2.4 GHz**，ESP8266 不支持 5 GHz） |
| `WIFI_PASSWORD` | ✅ | 同上 | WiFi 密码 |
| `MQTT_SERVER` | ✅ | `mqttClient.setServer(MQTT_SERVER, MQTT_PORT)` | broker 域名或 IP，示例占位 `broker.example.com` |
| `MQTT_PORT` | ✅ | 同上 | 端口，模板示例为 `1883`（明文，无 TLS） |
| `TOPIC_PUB` | ✅ | `mqttClient.publish(TOPIC_PUB, buf)` | **本节点发布**温湿度 |
| `TOPIC_SUB` | ✅ | `mqttClient.subscribe(TOPIC_SUB)` | **本节点订阅**下行；内容原样打到串口 |

**模板字段与代码用法完全一致，无需改动变量名。** 另外几个常量不放进 `secrets.h`（与部署环境无关）：

| 常量 | 代码值 | 位置 | 备注 |
| --- | --- | --- | --- |
| `MQTT_CLIENT_ID` | `"ESP8266_DHT11"` | 第 22 行 | **固定值，不含 MAC 后缀**；多台同固件设备会互相顶掉连接，见第 8 节 |
| `INTERVAL` | `1000` | 第 25 行 | 上报周期（毫秒） |
| `DHT_PIN` | `D4` | 第 26 行 | 数据引脚 |

> 模板 `secrets.h.example` 的注释建议：`TOPIC_PUB` 与 `TOPIC_SUB` 需与 K230 端 `secrets.py` 中的
> `MQTT_PUB_TOPIC` / `MQTT_TOPIC` **交叉对应**（对端发布 → 本端订阅），详见第 8 节的配对说明。

### 4.3 secrets.h 模板内容（示例值）

```c
#define WIFI_SSID       "your-wifi-ssid"
#define WIFI_PASSWORD   "your-wifi-password"
#define MQTT_SERVER     "broker.example.com"
#define MQTT_PORT       1883
#define TOPIC_PUB       "your_pub_topic"    // 本设备发布：{"temp":..,"humi":..}
#define TOPIC_SUB       "your_sub_topic"    // 本设备订阅：收到的内容原样打印到串口
```

> 公开仓库、截图、日志里请一律使用 `broker.example.com` / `192.0.2.10` 这类**文档保留值**占位，
> 不要泄露真实 broker 域名、服务器 IP 与 WiFi 密码。

---

## 5. MQTT 主题与报文格式

### 5.1 上行（本节点 → broker）

发布代码（第 98–101 行，逐字）：

```cpp
if (dht11.readTemperatureHumidity(t, h) == 0) {
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"temp\":%d,\"humi\":%d}", t, h);
    mqttClient.publish(TOPIC_PUB, buf);
}
```

| 项目 | 值 |
| --- | --- |
| 主题 | `TOPIC_PUB`（来自 `secrets.h`） |
| 载荷 | **紧凑 JSON，无空格**：`{"temp":25,"humi":60}` |
| JSON 字段 | `temp`（温度）、`humi`（湿度）—— **仅此两个字段** |
| 数据类型 | `%d` 整数（`int t, h`）。DHT11 本身分辨率为 1 ℃ / 1 %RH，整数够用 |
| 单位 | 温度 ℃、湿度 %RH —— **代码里没有带单位**，这是 DHT11 的惯例；**需作者确认** |
| 载荷长度 | 最长约 `{"temp":-10,"humi":100}` = 24 字节，`buf` 为 64 字节，安全 |
| QoS / retain | 代码未传参 → **QoS 0、不 retain**（PubSubClient 默认值） |
| 发布时机 | 每 1000 ms 一次，**且仅当 DHT11 读取成功** |

### 5.2 下行（broker → 本节点）

回调代码（第 108–115 行，逐字）：

```cpp
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char msg[256];
  unsigned int len = (length < sizeof(msg) - 1) ? length : (sizeof(msg) - 1);
  memcpy(msg, payload, len);
  msg[len] = '\0';
  Serial.println(msg);
}
```

| 项目 | 值 |
| --- | --- |
| 主题 | `TOPIC_SUB`（来自 `secrets.h`），**QoS 0**（`subscribe(TOPIC_SUB)` 未传 qos） |
| 载荷 | **任意字节，原样透传**：不解析、不校验、不判断主题 |
| 输出 | `Serial.println(msg)` —— 打印内容 + 换行；**主题名不会打印** |
| 长度上限 | **255 字节**（`char msg[256]`，超出部分被截断，多余字节丢弃） |
| 编码 | 直接按 `char` 输出，非 ASCII 内容在串口监视器里可能需要手动切编码 |
| 路由 | 回调不使用 `topic` 参数，所以**即使误订阅了别的主题，输出也看不出区别** |

---

## 6. 上报周期与运行流程

### 6.1 周期

- 上报周期常量：`const unsigned long INTERVAL = 1000;`（注释原文：「1秒上报一次」）。
- 判定方式：`if (millis() - lastTime >= INTERVAL)`，**先更新 `lastTime` 再调用 `publishData()`**，
  因此实际周期 ≈ 1 s + DHT11 读取耗时（DHT11 单次读取为毫秒级），并且**读取失败也不会重试**，
  要等下一个 1 s 周期。
- 提示：DHT11 的采样间隔通常要求 **≥ 1 s**（有的资料建议 2 s）。当前 `INTERVAL = 1000`
  正好卡在下限，若出现连续读到 0 或校验失败，可把 `INTERVAL` 调大到 2000。**具体建议值需作者确认。**

### 6.2 运行流程

```text
setup()
 ├─ Serial.begin(115200)
 ├─ WiFi.mode(WIFI_STA) → WiFi.begin(WIFI_SSID, WIFI_PASSWORD)
 │     └─ while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }   // 死等，无超时
 ├─ mqttClient.setServer(MQTT_SERVER, MQTT_PORT)
 ├─ mqttClient.setCallback(mqttCallback)
 └─ connectMQTT()
       └─ while (!mqttClient.connected()) {
              connect(MQTT_CLIENT_ID) 成功 → subscribe(TOPIC_SUB)，打印订阅主题
                                      失败 → Serial.print(" 失败, 5秒后重试"); delay(5000);
          }

loop()
 ├─ if (!mqttClient.connected()) connectMQTT();   // 断线自动重连（阻塞式）
 ├─ mqttClient.loop();                            // 收下行包 + 维持 keepalive
 └─ if (millis() - lastTime >= INTERVAL) { lastTime = millis(); publishData(); }
```

**keepalive**：代码**没有**显式调用 `setKeepAlive()`，因此使用 PubSubClient 库的默认值
（常见版本为 **15 秒**）；报文缓冲同样用库默认（常见为 256 字节）。
具体数值以你实际安装的库版本为准，**需作者确认**。

**已知的健壮性特点**（阅读代码即可确认，供二次开发参考）：

- WiFi 与 MQTT 的连接都是**阻塞式死等**，网络不可用时会卡在 `setup()` / `connectMQTT()` 里，
  期间 `loop()` 不会执行，也就不会采集温湿度。
- **WiFi 断开后没有重连逻辑**：`loop()` 只检查 `mqttClient.connected()`；
  WiFi 掉线会导致 MQTT 连接失效并触发重连，但代码不会主动 `WiFi.reconnect()`。
- MQTT 连接**未使用用户名/密码**（`mqttClient.connect(MQTT_CLIENT_ID)` 单参数形式），
  只能连允许匿名登录的 broker。
- 连接失败时**没有打印返回码**，只打印「失败, 5秒后重试」。调试时可自行加一行
  `Serial.println(mqttClient.state());`：PubSubClient 的返回码为
  `-4` 连接超时、`-3` 连接丢失、`-2` 连接失败、`-1` 已断开、`0` 已连接、
  `1` 协议版本错误、`2` 客户端 ID 被拒、`3` 服务不可用、`4` 用户名密码错误、`5` 未授权
  （**这些是库定义的常量，当前代码并未打印**）。

---

## 7. 编译与烧录

### 7.1 ⚠️ 先解决文件夹名问题（Arduino IDE 必读）

**Arduino IDE 强制要求：`.ino` 文件所在的文件夹名必须与 `.ino` 文件名完全一致。**

当前仓库里的实际布局是：

```text
ESP8266/
└── ESP8266_DHT11_MQTT.ino     ← 文件夹名 "ESP8266" ≠ 文件名 "ESP8266_DHT11_MQTT"
```

**两个解决办法，二选一：**

| 方案 | 操作 | 结果 |
| --- | --- | --- |
| **A（推荐）** 改文件夹名 | 把 `ESP8266/` 重命名为 `ESP8266_DHT11_MQTT/` | `ESP8266_DHT11_MQTT/ESP8266_DHT11_MQTT.ino` ✅ |
| **B** 改文件名 | 把 `ESP8266_DHT11_MQTT.ino` 重命名为 `ESP8266.ino` | `ESP8266/ESP8266.ino` ✅，但丢失了自解释的文件名 |

如果两者都不做，直接双击打开 `.ino`，Arduino IDE 会提示
「**该文件需要放在名为 `ESP8266_DHT11_MQTT` 的 sketch 文件夹中**」并主动提出帮你新建文件夹/重命名；
若强行编译，常见报错是找不到 sketch 主文件或提示目录名不匹配。
另外，**重命名后 `secrets.h` 必须跟着一起移动**，它靠 `#include "secrets.h"` 同目录查找。

### 7.2 Arduino IDE 步骤

1. **安装开发板支持**：文件 → 首选项 → 附加开发板管理器网址，填入
   `http://arduino.esp8266.com/stable/package_esp8266com_index.json`；
   然后 工具 → 开发板 → 开发板管理器，搜索并安装 **esp8266**。
2. **安装库**：工具 → 管理库，搜索安装
   - **PubSubClient**（作者 Nick O'Leary）
   - **DHT11** 库 —— **需作者确认具体库名/作者**，必须提供
     `DHT11(int pin)` 构造函数与 `readTemperatureHumidity(int&, int&)` 方法（返回 0 表示成功）。
3. **按 7.1 处理文件夹名**，然后打开草图。
4. **创建配置**：把 `secrets.h.example` 复制为同目录的 `secrets.h`，填入 WiFi 与 MQTT 信息。
5. **选择开发板与端口**：工具 → 开发板（如 *NodeMCU 1.0 (ESP-12E Module)* 或
   *LOLIN(WEMOS) D1 R2 & mini*，按实际硬件选）→ 端口。
6. **上传**：点击上传箭头。**上传前请关闭串口监视器**（ESP8266 只有一个串口，被占用会导致上传失败）。
7. **查看日志**：工具 → 串口监视器，波特率选 **115200**。正常可见：

   ```text
   [WiFi] 连接中....
   [WiFi] 已连接, IP: 192.0.2.10
   [MQTT] 连接中... 成功
   [MQTT] 订阅: your_sub_topic
   ```

   之后向 `TOPIC_SUB` 发消息，原文就会出现在串口里。

### 7.3 PlatformIO 步骤

PlatformIO 可以直接编译 `src/` 下的 `.ino`（会自动做 Arduino 风格预处理），
也可以改名为 `src/main.cpp` 并在首行加 `#include <Arduino.h>`（**推荐后者，行为更可预期**）。
目录建议：

```text
ESP8266_DHT11_MQTT/
├── platformio.ini
├── include/
│   └── secrets.h          # 与草图同目录或放 include/，二选一，需与 #include 写法一致
└── src/
    └── main.cpp           # 由 ESP8266_DHT11_MQTT.ino 改名而来
```

`platformio.ini` 参考（**板子型号与 DHT11 库名需作者确认**）：

```ini
[env:esp8266]
platform  = espressif8266
board     = nodemcuv2          ; 按实际板子修改（如 d1_mini）
framework = arduino
monitor_speed = 115200
lib_deps  =
    knolleary/PubSubClient
    ; DHT11 库：按实际所用库填写（需作者确认）
```

> 若把 `secrets.h` 放在 `include/` 目录，需确认编译时该目录在头文件搜索路径中
> （PlatformIO 默认 `include/` 在路径里，但草图用的是 `#include "secrets.h"`，
> 相对当前文件查找会优先，**建议就放在与源文件同一目录**，避免歧义）。

---

## 8. 与 K230 主题配对的注意事项

本节点与 K230 视觉节点（见 [`../K230/README.md`](../K230/README.md)）**共用同一个 broker**，
主题配对是联调最容易出错的地方。两侧事实如下：

| | 订阅 | 发布 |
| --- | --- | --- |
| **ESP8266** | `TOPIC_SUB`（`secrets.h`） | `TOPIC_PUB`（`secrets.h`），载荷 `{"temp":..,"humi":..}` |
| **K230** | `MQTT_TOPIC`（`secrets.py`） | `MQTT_PUB_TOPIC`（`secrets.py`），上线时发 `OK` |

**配对规则：**

1. **想让 ESP8266 收到 K230 的动向**：把 ESP8266 的 `TOPIC_SUB` 设成与 K230 的 `MQTT_TOPIC`
   **完全相同**。此时 K230 收到的下行指令，ESP8266 也会收到并打印到串口。
2. **不要把 ESP8266 的 `TOPIC_PUB` 设成 K230 的 `MQTT_TOPIC`**：那样每秒一条温湿度 JSON 会喂给
   K230 的 `mqtt_cmd_callback`，匹配不上任何指令，只会不停打印
   `MQTT rx ignored (noise?):` —— 程序不会崩，但日志被刷屏。
   温湿度请发到**独立主题**（例如模板里的 `your_pub_topic`）。
3. **不要把 ESP8266 的 `TOPIC_SUB` 设成 K230 的 `MQTT_PUB_TOPIC`**：K230 每次上线都会往那儿发一个
   `OK`，ESP8266 会在串口里莫名奇妙打印一行 `OK`。
4. **两者之间没有直接通信**：ESP8266 代码**没有任何向 K230 回发指令的路径**，
   它只发布温湿度、只打印下行内容。如果你希望「串口输入 → 转发给 K230」，
   需要自行在 `mqttCallback()` 里增加 `mqttClient.publish(<K230 的订阅主题>, msg)` 之类的逻辑
   （**当前代码没有实现**）。
5. **认证方式必须一致**：ESP8266 用单参数 `connect(MQTT_CLIENT_ID)`，**不带账号密码**；
   K230 的 `MQTT_USER` / `MQTT_PASS` 默认也是 `None`。若 broker 要求鉴权，
   K230 只需在 `secrets.py` 里填 `MQTT_USER` / `MQTT_PASS`（代码已把它们传给 `MQTTClient`），
   ESP8266 则需要把 `connect()` 改成带账号密码的版本。
6. **客户端 ID 冲突**：ESP8266 的 `MQTT_CLIENT_ID` 是固定的 `"ESP8266_DHT11"`，
   烧录多块相同固件的板子会互相踢下线；K230 侧会自动追加 MAC 后缀来规避。
   多设备部署时请自行给 ESP8266 加上唯一后缀（例如拼上 `WiFi.macAddress()`）。
7. **端口**：两侧都用 **1883** 明文 MQTT，**未启用 TLS**，不要指向 8883。

---

## 9. 常见问题（FAQ）

### Q1. 串口一直是 `[MQTT] 连接中... 失败, 5秒后重试`

① 检查 `MQTT_SERVER` / `MQTT_PORT` 是否可达（用手机上的 MQTT 客户端交叉验证）；
② broker 是否允许**匿名**连接（代码不带账号密码）；
③ 客户端 ID 是否与在线的另一台设备重复（默认固定为 `ESP8266_DHT11`）；
④ 自行加 `Serial.println(mqttClient.state());` 看库返回码（见 6.2 节）。

### Q2. WiFi 一直打印点号，连不上

ESP8266 **只支持 2.4 GHz**，请确认 SSID 是 2.4 GHz 频段；另外注意
`WIFI_SSID` 区分大小写、不要带入多余空格；企业级认证（WPA2-Enterprise）本代码不支持。
代码里 WiFi 等待是**死循环、没有超时**，连不上就永远不会往下走。

### Q3. 串口监视器里是乱码

波特率不匹配。代码用 `Serial.begin(115200)`，串口监视器右下角也要选 **115200**。

### Q4. 温度/湿度永远是 0，或者没有上报

`publishData()` **只在 `readTemperatureHumidity(t, h) == 0` 时才发布**，读取失败是完全静默的。
排查：① `DHT_PIN = D4`（GPIO2）接线是否正确、是否共地；② 供电是否 3.3V、DAT 是否需要上拉电阻；
③ 采样间隔是否过短（可把 `INTERVAL` 从 1000 调到 2000 试试）；④ 换一片 DHT11 排除器件损坏。

### Q5. 收不到下行消息

① 确认发布方用的主题与 `TOPIC_SUB` **逐字符一致**（MQTT 主题区分大小写、`/` 位置敏感）；
② 确认 `loop()` 里 `mqttClient.loop()` 在正常运行（它负责收包）；
③ 消息不要超过 **255 字节**，否则会被截断；
④ 回调**不打印主题名**，如果收到的是自己发的温湿度回环也会直接打印 JSON —— 用内容判断来源。

### Q6. 编译报错找不到 `DHT11.h` 或 `readTemperatureHumidity` 未定义

装错了 DHT11 库。市面上有多个同名库、API 不同，必须选提供
`DHT11 dht11(pin)` + `readTemperatureHumidity(int&, int&)`（返回 0 = 成功）的那一个。
**具体库名与作者需作者确认**，确认后建议在 README 里写死版本号，避免后人踩坑。

### Q7. 上传失败 / 找不到端口

ESP8266 开发板需要用支持自动复位的 USB 线；上传前**关闭串口监视器**；
Windows 下若缺驱动请安装板载 USB-UART 芯片（CH340 / CP2102）的驱动。

---

## 10. 许可

本项目以 **MIT** 协议发布，见仓库根目录 [`LICENSE`](../LICENSE)。
（本文按「README 位于子项目目录 `ESP8266/`、`LICENSE` 与 `.gitignore` 位于其上一级仓库根目录」的相对位置书写，
若你的目录层级不同，请相应调整链接。）

第三方库（**ESP8266 Arduino Core**、**PubSubClient**、以及所用 **DHT11** 库）各自遵循其原有许可，
本仓库不重新分发它们的源码，仅通过 `#include` 引用，请在使用与再发布时遵守各自的许可条款。
