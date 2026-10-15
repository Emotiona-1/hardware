import os
import ujson
import aicube
from media.sensor import *
from media.display import *
from media.media import *
from time import *
import nncase_runtime as nn
import ulab.numpy as np
import time
import utime
import image
import random
import gc
import network
import socket
import struct
from machine import UART, FPIOA

try:
    from umqtt.simple import MQTTClient
    MQTT_AVAILABLE = True
except ImportError:
    MQTT_AVAILABLE = False
    print("WARNING: umqtt.simple not found, MQTT will be disabled")

display_mode="hdmi"
if display_mode=="lcd":
    DISPLAY_WIDTH = ALIGN_UP(800, 16)
    DISPLAY_HEIGHT = 480
else:
    DISPLAY_WIDTH = ALIGN_UP(1920, 16)
    DISPLAY_HEIGHT = 1080

OUT_RGB888P_WIDTH = ALIGN_UP(1080, 16)
OUT_RGB888P_HEIGHT = 720

# ──────────────── 本地凭据（不入库） ────────────────
# WiFi 密码、服务器地址、MQTT broker 与主题都放在同目录的 secrets.py（已加入 .gitignore）。
# 首次使用请把 secrets.py.example 复制为 secrets.py 并填入自己的值。
try:
    from secrets import (
        WIFI_SSID, WIFI_PASS, TCP_SERVER_IP, TCP_SERVER_PORT,
        MQTT_BROKER, MQTT_PORT, MQTT_TOPIC, MQTT_PUB_TOPIC,
        MQTT_USER, MQTT_PASS,
    )
except ImportError:
    raise ImportError(
        "缺少本地配置文件：请将 secrets.py.example 复制为 secrets.py 并填入 WiFi、服务器与 MQTT 信息"
    )
JPEG_QUALITY = 40
DISEASE_SEND_INTERVAL_SEC = 10

UART2_BAUDRATE = 9600
F_O_INTERVAL_SEC = 3

# MQTT 服务器地址与主题来自 secrets.py，此处只保留非敏感的客户端参数
MQTT_CLIENT_ID = "k230_lastk230"
MQTT_KEEPALIVE = 60


FRAME_HEADER = b'\xAA\xBB\xCC\xDD'
FRAME_FOOTER = b'\xEE\xFF\x00\xFF'

color_four = [(255, 220, 20, 60), (255, 119, 11, 32), (255, 0, 0, 142), (255, 0, 0, 230),
        (255, 106, 0, 228), (255, 0, 60, 100), (255, 0, 80, 100), (255, 0, 0, 70),
        (255, 0, 0, 192), (255, 250, 170, 30), (255, 100, 170, 30), (255, 220, 220, 0),
        (255, 175, 116, 175), (255, 250, 0, 30), (255, 165, 42, 42), (255, 255, 77, 255),
        (255, 0, 226, 252), (255, 182, 182, 255), (255, 0, 82, 0), (255, 120, 166, 157),
        (255, 110, 76, 0), (255, 174, 57, 255), (255, 199, 100, 0), (255, 72, 0, 118),
        (255, 255, 179, 240), (255, 0, 125, 92), (255, 209, 0, 151), (255, 188, 208, 182),
        (255, 0, 220, 176), (255, 255, 99, 164), (255, 92, 0, 73), (255, 133, 129, 255),
        (255, 78, 180, 255), (255, 0, 228, 0), (255, 174, 255, 243), (255, 45, 89, 255),
        (255, 134, 134, 103), (255, 145, 148, 174), (255, 255, 208, 186),
        (255, 197, 226, 255), (255, 171, 134, 1), (255, 109, 63, 54), (255, 207, 138, 255),
        (255, 151, 0, 95), (255, 9, 80, 61), (255, 84, 105, 51), (255, 74, 65, 105),
        (255, 166, 196, 102), (255, 208, 195, 210), (255, 255, 109, 65), (255, 0, 143, 149),
        (255, 179, 0, 194), (255, 209, 99, 106), (255, 5, 121, 0), (255, 227, 255, 205),
        (255, 147, 186, 208), (255, 153, 69, 1), (255, 3, 95, 161), (255, 163, 255, 0),
        (255, 119, 0, 170), (255, 0, 182, 199), (255, 0, 165, 120), (255, 183, 130, 88),
        (255, 95, 32, 0), (255, 130, 114, 135), (255, 110, 129, 133), (255, 166, 74, 118),
        (255, 219, 142, 185), (255, 79, 210, 114), (255, 178, 90, 62), (255, 65, 70, 15),
        (255, 127, 167, 115), (255, 59, 105, 106), (255, 142, 108, 45), (255, 196, 172, 0),
        (255, 95, 54, 80), (255, 128, 76, 255), (255, 201, 57, 1), (255, 246, 0, 122),
        (255, 191, 162, 208)]

root_path="/sdcard/the_model/"
config_path=root_path+"deploy_config.json"
deploy_conf={}
debug_mode=0

CLASS_FLOWER  = 0
CLASS_FRUIT   = 1
CLASS_DISEASE = 2

tcp_socket  = None
uart        = None
uart2       = None
wlan        = None
mqtt_client = None
mqtt_last_ping = 0
mqtt_client_id = MQTT_CLIENT_ID
photo_count = 0
boot_cmd_received = False


class ScopedTiming:
    def __init__(self, info="", enable_profile=True):
        self.info = info
        self.enable_profile = enable_profile

    def __enter__(self):
        if self.enable_profile:
            self.start_time = time.time_ns()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        if self.enable_profile:
            elapsed_time = time.time_ns() - self.start_time
            print(f"{self.info} took {elapsed_time / 1000000:.2f} ms")


def read_deploy_config(config_path):
    with open(config_path, 'r') as json_file:
        try:
            config = ujson.load(json_file)
        except ValueError as e:
            print("JSON 解析错误:", e)
    return config


def connect_wifi(ssid, password):
    global wlan
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.connect(ssid, password)
    timeout = 20
    while not wlan.isconnected() and timeout > 0:
        utime.sleep(0.5)
        timeout -= 0.5
    if wlan.isconnected():
        print("WiFi OK, IP:", wlan.ifconfig()[0])
        return True
    else:
        print("WiFi connect failed")
        return False


def connect_tcp_server(ip, port):
    global tcp_socket
    if tcp_socket:
        try:
            tcp_socket.close()
        except:
            pass
    try:
        tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        tcp_socket.settimeout(5)
        tcp_socket.connect((ip, port))
        tcp_socket.setblocking(True)  # 连接后恢复阻塞模式，保证 sendall 完整发出
        print("TCP OK:", ip, port)
        return True
    except Exception as e:
        print("TCP connect failed:", e)
        tcp_socket = None
        return False


def send_tcp_image(sock, img):
    global photo_count
    try:
        jpeg_data = bytes(img.to_jpeg(quality=JPEG_QUALITY))
        gc.collect()

        if len(jpeg_data) < 2 or jpeg_data[0] != 0xFF or jpeg_data[1] != 0xD8:
            print(f"WARNING: Invalid JPEG header! First bytes: {jpeg_data[:4].hex() if jpeg_data else 'empty'}")
            return 0

        packet = FRAME_HEADER + len(jpeg_data).to_bytes(4, 'big') + jpeg_data + FRAME_FOOTER
        sock.sendall(packet)
        photo_count += 1
        print(f"TCP sent #{photo_count}, size={len(jpeg_data)}B")
        return len(jpeg_data)
    except Exception as e:
        print("TCP send failed:", e)
        return 0

def mqtt_cmd_callback(topic, msg):
    global uart, uart2, boot_cmd_received
    try:
        cmd = msg.strip()  # 去除 \r \n 等空白
        if cmd == b"1":
            if uart is not None:
                for _ in range(4):
                    uart.write("one\n")
                print("MQTT '1' -> UART1 'one' x4")
            else:
                print("WARNING: uart is None, cannot forward '1'")
            boot_cmd_received = True
        elif cmd == b"2":
            if uart is not None:
                for _ in range(4):
                    uart.write("two\n")
                print("MQTT '2' -> UART1 'two' x4")
            else:
                print("WARNING: uart is None, cannot forward '2'")
            boot_cmd_received = True
        elif cmd == b"3":
            if uart is not None:
                for _ in range(4):
                    uart.write("three\n")
                print("MQTT '3' -> UART1 'three' x4")
            else:
                print("WARNING: uart is None, cannot forward '3'")
            boot_cmd_received = True
        elif cmd in (b"s", b"l", b"r", b"f", b"b"):
            mapping = {b"s": '{"command":"stop"}', b"l": '{"command":"left"}', b"r": '{"command":"right"}',
                       b"f": '{"command":"forward"}', b"b": '{"command":"backward"}'}
            send_str = mapping[cmd]
            if uart2 is not None:
                uart2.write(send_str)
                print("MQTT '{}' -> UART2 send {}".format(cmd.decode(), send_str))
            else:
                print("WARNING: uart2 is None, cannot send {}".format(send_str))
        elif cmd != b"":
            print("MQTT rx ignored (noise?):", msg.hex())
    except Exception as e:
        print("MQTT callback error:", e)


# ============ MQTT 连接 ============
def connect_mqtt():
    global mqtt_client, mqtt_last_ping, mqtt_client_id
    if not MQTT_AVAILABLE:
        print("MQTT not available (umqtt.simple missing)")
        return False
    try:
        try:
            addr_info = socket.getaddrinfo(MQTT_BROKER, MQTT_PORT)
            broker_ip = addr_info[0][-1][0]   # 解析出的数字 IP
            print("MQTT DNS OK:", MQTT_BROKER, "→", broker_ip)
        except Exception as dns_err:
            print("MQTT DNS resolve failed:", MQTT_BROKER, "→", repr(dns_err))
            print("  提示：K230 无可用 DNS 时，可在 secrets.py 里把 MQTT_BROKER 直接改成 broker 的数字 IP。")
            return False

        if mqtt_client is not None:
            try:
                mqtt_client.disconnect()
            except:
                pass
            mqtt_client = None

        if mqtt_client_id == MQTT_CLIENT_ID:
            try:
                import ubinascii
                mac = wlan.config('mac')
                mqtt_client_id = MQTT_CLIENT_ID + "_" + ubinascii.hexlify(mac[-3:]).decode()
            except Exception:
                mqtt_client_id = MQTT_CLIENT_ID + "_" + str(random.randint(1000, 99999))

        mqtt_client = MQTTClient(mqtt_client_id, broker_ip, port=MQTT_PORT,
                                 user=MQTT_USER, password=MQTT_PASS,
                                 keepalive=MQTT_KEEPALIVE)
        mqtt_client.set_callback(mqtt_cmd_callback)
        mqtt_client.connect()
        mqtt_client.subscribe(MQTT_TOPIC)
        mqtt_last_ping = utime.time()   # 从连接成功这一刻开始计心跳
        try:
            mqtt_client.publish(MQTT_PUB_TOPIC, b"OK")
            print("MQTT published OK ->", MQTT_PUB_TOPIC)
        except Exception as pub_err:
            print("MQTT publish OK failed:", pub_err)
        print("MQTT OK:", MQTT_BROKER, MQTT_PORT, "topic:", MQTT_TOPIC, "id:", mqtt_client_id)
        return True
    except Exception as e:
        print("MQTT connect failed:", repr(e))
        try:
            if mqtt_client is not None:
                mqtt_client.disconnect()
        except:
            pass
        mqtt_client = None
        return False


def mqtt_keepalive():
    global mqtt_client, mqtt_last_ping
    if mqtt_client is None:
        return
    now = utime.time()
    if now - mqtt_last_ping >= MQTT_KEEPALIVE // 2:
        try:
            mqtt_client.ping()
            mqtt_last_ping = now
        except Exception as e:
            print("MQTT ping failed, connection dead:", e)
            try:
                mqtt_client.disconnect()
            except:
                pass
            mqtt_client = None


def wait_for_mqtt_cmd():
    global mqtt_client, wlan, boot_cmd_received
    boot_cmd_received = False
    print("Waiting for MQTT connection and boot command (1/2/3) ...")
    last_reconnect_attempt = 0
    while not boot_cmd_received:
        now = utime.time()

        if wlan is None or not wlan.isconnected():
            if now - last_reconnect_attempt >= 3:
                last_reconnect_attempt = now
                print("WiFi not connected, trying to connect...")
                connect_wifi(WIFI_SSID, WIFI_PASS)
                if wlan is not None and wlan.isconnected():
                    connect_tcp_server(TCP_SERVER_IP, TCP_SERVER_PORT)
                    connect_mqtt()
        elif mqtt_client is None:
            if now - last_reconnect_attempt >= 3:
                last_reconnect_attempt = now
                print("MQTT not connected, trying to connect...")
                connect_mqtt()

        if mqtt_client is not None:
            try:
                mqtt_client.check_msg()
            except Exception as e:
                print("MQTT check_msg failed:", e)
                try:
                    mqtt_client.disconnect()
                except:
                    pass
                mqtt_client = None

        mqtt_keepalive()
        utime.sleep(0.05)
    print("MQTT command received, continue boot.")
    return True


def detection():
    global tcp_socket, uart, uart2, wlan, mqtt_client

    print("det_infer start")

    deploy_conf = read_deploy_config(config_path)
    kmodel_name         = deploy_conf["kmodel_path"]
    labels              = deploy_conf["categories"]
    confidence_threshold = deploy_conf["confidence_threshold"]
    nms_threshold       = deploy_conf["nms_threshold"]
    img_size            = deploy_conf["img_size"]
    num_classes         = deploy_conf["num_classes"]
    nms_option          = deploy_conf["nms_option"]
    model_type          = deploy_conf["model_type"]
    if model_type == "AnchorBaseDet":
        anchors = deploy_conf["anchors"][0] + deploy_conf["anchors"][1] + deploy_conf["anchors"][2]
    kmodel_frame_size = img_size
    frame_size = [OUT_RGB888P_WIDTH, OUT_RGB888P_HEIGHT]
    strides = [8, 16, 32]

    ori_w = OUT_RGB888P_WIDTH
    ori_h = OUT_RGB888P_HEIGHT
    width  = kmodel_frame_size[0]
    height = kmodel_frame_size[1]
    ratiow = float(width) / ori_w
    ratioh = float(height) / ori_h
    if ratiow < ratioh:
        ratio = ratiow
    else:
        ratio = ratioh
    new_w = int(ratio * ori_w)
    new_h = int(ratio * ori_h)
    dw = float(width - new_w) / 2
    dh = float(height - new_h) / 2
    top    = int(round(dh - 0.1))
    bottom = int(round(dh + 0.1))
    left   = int(round(dw - 0.1))
    right  = int(round(dw + 0.1))

    kpu = nn.kpu()
    ai2d = nn.ai2d()
    kpu.load_kmodel(root_path + kmodel_name)
    ai2d.set_dtype(nn.ai2d_format.NCHW_FMT,
                   nn.ai2d_format.NCHW_FMT,
                   np.uint8, np.uint8)
    ai2d.set_pad_param(True, [0, 0, 0, 0, top, bottom, left, right], 0, [114, 114, 114])
    ai2d.set_resize_param(True, nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
    ai2d_builder = ai2d.build([1, 3, OUT_RGB888P_HEIGHT, OUT_RGB888P_WIDTH],
                              [1, 3, height, width])

    uart = None
    try:
        fpioa = FPIOA()
        fpioa.set_function(3, FPIOA.UART1_TXD)
        fpioa.set_function(4, FPIOA.UART1_RXD)
        uart = UART(UART.UART1, baudrate=115200, bits=UART.EIGHTBITS, parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE)
        print("UART1 OK, GPIO3(TX) GPIO4(RX), baudrate=115200")
    except Exception as e:
        print("UART init failed:", e)
        uart = None

    uart2 = None
    try:
        fpioa.set_function(11, FPIOA.UART2_TXD)
        fpioa.set_function(12, FPIOA.UART2_RXD)
        uart2 = UART(UART.UART2, baudrate=UART2_BAUDRATE, bits=UART.EIGHTBITS, parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE)
        print("UART2 OK, IO11(TX2) IO12(RX2), baudrate={}".format(UART2_BAUDRATE))
    except Exception as e:
        print("UART2 init failed:", e)
        uart2 = None

    if connect_wifi(WIFI_SSID, WIFI_PASS):
        try:
            addr_info_tcp = socket.getaddrinfo(TCP_SERVER_IP, TCP_SERVER_PORT)
            print("TCP DNS OK:", TCP_SERVER_IP, "→", addr_info_tcp[0][4])
        except Exception as e:
            print("TCP DNS resolve failed:", e)

        connect_tcp_server(TCP_SERVER_IP, TCP_SERVER_PORT)
        connect_mqtt()   # MQTT 接收取代 UART2 接收，WiFi 连上后立即连接订阅
    else:
        print("WiFi not connected, skip TCP/MQTT init")

    wait_for_mqtt_cmd()

    sensor = Sensor()
    sensor.reset()
    sensor.set_hmirror(False)
    sensor.set_vflip(False)
    sensor.set_framesize(width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT)
    sensor.set_pixformat(PIXEL_FORMAT_YUV_SEMIPLANAR_420)
    sensor.set_framesize(width=OUT_RGB888P_WIDTH, height=OUT_RGB888P_HEIGHT, chn=CAM_CHN_ID_2)
    sensor.set_pixformat(PIXEL_FORMAT_RGB_888_PLANAR, chn=CAM_CHN_ID_2)
    sensor_bind_info = sensor.bind_info(x=0, y=0, chn=CAM_CHN_ID_0)
    Display.bind_layer(**sensor_bind_info, layer=Display.LAYER_VIDEO1)
    if display_mode == "lcd":
        Display.init(Display.ST7701, to_ide=True)
    else:
        Display.init(Display.LT9611, to_ide=True)

    osd_img = image.Image(DISPLAY_WIDTH, DISPLAY_HEIGHT, image.ARGB8888)

    last_disease_send_time = 0        # 上一次 TCP 上传病害图片的时间戳
    disease_detected_in_frame = False # 当前帧是否检测到 tomato_disease
    last_wifi_check = 0               # WiFi定时检测时间戳
    flower_send_enabled = False       # 是否允许发送 flower 坐标（上电后须完成 f→o 循环才置 True）
    f_o_phase = "IDLE"                # f/o 循环状态：IDLE / WAIT_AFTER_F
    f_sent_time = 0                   # 上一次发送 'f' 的时间戳

    MediaManager.init()
    sensor.run()

    if uart2 is not None:
        uart2.write('{"command":"forward"}')
        print('UART2 sent {"command":"forward"} (boot sequence)')
    f_o_phase = "WAIT_AFTER_F"
    f_sent_time = utime.time()

    rgb888p_img = None
    ai2d_input_tensor = None
    data = np.ones((1, 3, width, height), dtype=np.uint8)
    ai2d_output_tensor = nn.from_numpy(data)

    while True:
        frame_start = utime.time()
        try:
            with ScopedTiming("total", debug_mode > 0):
                rgb888p_img = sensor.snapshot(chn=CAM_CHN_ID_2)

                if frame_start - last_wifi_check >= 10:
                    last_wifi_check = frame_start
                    if wlan is None or not wlan.isconnected():
                        print("WiFi lost, reconnect...")
                        connect_wifi(WIFI_SSID, WIFI_PASS)
                        if wlan.isconnected():
                            connect_tcp_server(TCP_SERVER_IP, TCP_SERVER_PORT)
                            connect_mqtt()
                    elif mqtt_client is None:
                        connect_mqtt()

                if uart is not None and uart.any():
                    try:
                        rx_data = uart.read()
                        if rx_data and b"workOK" in rx_data:
                            flower_send_enabled = False
                            if uart2 is not None:
                                uart2.write('{"command":"forward"}')
                                print('UART2 sent {"command":"forward"} (workOK cycle)')
                            f_o_phase = "WAIT_AFTER_F"
                            f_sent_time = utime.time()
                            print("UART rx workOK, restart f/o cycle")
                    except Exception as e:
                        print("UART read failed:", e)

                if mqtt_client is not None:
                    try:
                        mqtt_client.check_msg()
                    except Exception as e:
                        print("MQTT check_msg failed:", e)
                        try:
                            mqtt_client.disconnect()
                        except:
                            pass
                        mqtt_client = None  # 置空后由每 10 秒的 WiFi 检查里自动重连

                mqtt_keepalive()

                now = utime.time()
                if f_o_phase == "WAIT_AFTER_F" and (now - f_sent_time >= F_O_INTERVAL_SEC):
                    if uart2 is not None:
                        uart2.write('{"command":"stop"}')
                        print('UART2 sent {"command":"stop"}, flower send enabled')
                    f_o_phase = "IDLE"
                    flower_send_enabled = True

                if rgb888p_img.format() == image.RGBP888:
                    ai2d_input = rgb888p_img.to_numpy_ref()
                    ai2d_input_tensor = nn.from_numpy(ai2d_input)
                    ai2d_builder.run(ai2d_input_tensor, ai2d_output_tensor)

                    kpu.set_input_tensor(0, ai2d_output_tensor)
                    kpu.run()

                    results = []
                    for i in range(kpu.outputs_size()):
                        out_data = kpu.get_output_tensor(i)
                        result = out_data.to_numpy()
                        result = result.reshape((result.shape[0] * result.shape[1] *
                                                 result.shape[2] * result.shape[3]))
                        del out_data
                        results.append(result)
                    gc.collect()

                    if model_type == "AnchorBaseDet":
                        det_boxes = aicube.anchorbasedet_post_process(
                            results[0], results[1], results[2],
                            kmodel_frame_size, frame_size, strides,
                            num_classes, confidence_threshold, nms_threshold,
                            anchors, nms_option)
                    elif model_type == "GFLDet":
                        det_boxes = aicube.gfldet_post_process(
                            results[0], results[1], results[2],
                            kmodel_frame_size, frame_size, strides,
                            num_classes, confidence_threshold, nms_threshold,
                            nms_option)
                    else:
                        det_boxes = aicube.anchorfreedet_post_process(
                            results[0], results[1], results[2],
                            kmodel_frame_size, frame_size, strides,
                            num_classes, confidence_threshold,
                            nms_option)

                    osd_img.clear()
                    disease_detected_in_frame = False

                    if det_boxes:
                        for det_boxe in det_boxes:
                            class_id = det_boxe[0]
                            score    = det_boxe[1]
                            x1, y1, x2, y2 = det_boxe[2], det_boxe[3], det_boxe[4], det_boxe[5]
                            label = labels[class_id]

                            x_center = (x1 + x2) / 2.0
                            y_center = (y1 + y2) / 2.0

                            w = float(x2 - x1) * DISPLAY_WIDTH // OUT_RGB888P_WIDTH
                            h = float(y2 - y1) * DISPLAY_HEIGHT // OUT_RGB888P_HEIGHT
                            osd_img.draw_rectangle(
                                int(x1 * DISPLAY_WIDTH // OUT_RGB888P_WIDTH),
                                int(y1 * DISPLAY_HEIGHT // OUT_RGB888P_HEIGHT),
                                int(w), int(h),
                                color=color_four[class_id][1:])
                            score_str = str(round(score, 2))
                            osd_img.draw_string_advanced(
                                int(x1 * DISPLAY_WIDTH // OUT_RGB888P_WIDTH),
                                int(y1 * DISPLAY_HEIGHT // OUT_RGB888P_HEIGHT) - 50,
                                32, label + " " + score_str,
                                color=color_four[class_id][1:])

                            if class_id == CLASS_FLOWER and flower_send_enabled:
                                mapped_x = int(-(x_center / OUT_RGB888P_WIDTH * 180 - 90))
                                mapped_y = int(y_center / OUT_RGB888P_HEIGHT * 200)
                                print("DEBUG: flower detected, uart={}, ready to send X:{}, Y:{}".format(uart, mapped_x, mapped_y))
                                if uart is not None:
                                    try:
                                        uart.write(str(mapped_x) + "," + str(mapped_y) + "\n")
                                        print("UART sent flower X:{}, Y:{}".format(mapped_x, mapped_y))
                                        flower_send_enabled = False
                                    except Exception as e:
                                        print("UART write failed:", e)
                                else:
                                    print("WARNING: uart is None, cannot send flower coordinates!")

                            if class_id == CLASS_DISEASE:
                                disease_detected_in_frame = True

                    current_time = utime.time()
                    if disease_detected_in_frame and (current_time - last_disease_send_time >= DISEASE_SEND_INTERVAL_SEC):
                        if tcp_socket is not None:
                            try:
                                yuv_img = sensor.snapshot(chn=CAM_CHN_ID_0)
                                if yuv_img is not None:
                                    send_tcp_image(tcp_socket, yuv_img)
                                    yuv_img = None
                                    gc.collect()
                                last_disease_send_time = current_time
                            except Exception as e:
                                print("TCP send error:", e)
                                try:
                                    tcp_socket.close()
                                except:
                                    pass
                                tcp_socket = None
                                if wlan is not None and wlan.isconnected():
                                    connect_tcp_server(TCP_SERVER_IP, TCP_SERVER_PORT)

                    Display.show_image(osd_img, 0, 0, Display.LAYER_OSD3)

                    del results
                    gc.collect()

                rgb888p_img = None
                del ai2d_input_tensor
                gc.collect()
        except Exception as frame_err:
            print("Single frame error skip:", frame_err)
            rgb888p_img = None
            gc.collect()

    try:
        os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)
        del ai2d_input_tensor
        del ai2d_output_tensor
        sensor.stop()
        Display.deinit()
        MediaManager.deinit()
        if tcp_socket:
            try:
                tcp_socket.close()
            except:
                pass
        if mqtt_client:
            try:
                mqtt_client.disconnect()
            except:
                pass
            mqtt_client = None
        if wlan:
            try:
                wlan.disconnect()
                wlan.active(False)
            except:
                pass
        gc.collect()
        time.sleep(1)
        nn.shrink_memory_pool()
    except:
        pass

    print("det_infer end")
    return 0


if __name__ == "__main__":
    detection()
