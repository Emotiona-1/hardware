import usocket as socket
import ustruct as struct
import utime


class MQTTException(Exception):
    pass


class MQTTClient:

    def __init__(self, client_id, server, port=0, user=None, password=None, keepalive=0,
                 ssl=False, ssl_params={}):
        if port == 0:
            port = 8883 if ssl else 1883
        self.client_id = client_id
        self.sock = None
        self.server = server
        self.port = port
        self.ssl = ssl
        self.ssl_params = ssl_params
        self.pid = 0
        self.cb = None
        self.user = user
        self.pswd = password
        self.keepalive = keepalive
        self.lw_topic = None
        self.lw_msg = None
        self.lw_qos = 0
        self.lw_retain = False
        self._read_timeout = 10

    def _send_str(self, s):
        self.sock.write(struct.pack("!H", len(s)))
        self.sock.write(s)

    def _read(self, n):
        data = b""
        deadline = utime.time() + self._read_timeout
        while len(data) < n:
            b = self.sock.read(n - len(data))
            if b is None:
                if utime.time() > deadline:
                    raise MQTTException("read timeout")
                utime.sleep_ms(5)
            elif b == b"":
                raise MQTTException("connection closed")
            else:
                data += b
        return data

    def _recv_len(self):
        n = 0
        sh = 0
        while 1:
            b = self._read(1)[0]
            n |= (b & 0x7f) << sh
            if not b & 0x80:
                return n
            sh += 7

    def set_callback(self, f):
        self.cb = f

    def set_last_will(self, topic, msg, retain=False, qos=0):
        assert 0 <= qos <= 2
        assert topic
        self.lw_topic = topic
        self.lw_msg = msg
        self.lw_qos = qos
        self.lw_retain = retain

    def connect(self, clean_session=True):
        self.sock = socket.socket()
        self.sock.settimeout(10)
        addr = socket.getaddrinfo(self.server, self.port)[0][-1]
        self.sock.connect(addr)
        self.sock.setblocking(True)
        if self.ssl:
            import ussl
            self.sock = ussl.wrap_socket(self.sock, **self.ssl_params)

        premsg = bytearray(b"\x10\0\0\0\0\0")
        msg = bytearray(b"\x04MQTT\x04\x02\0\0")

        sz = 10 + 2 + len(self.client_id)
        msg[6] = sz >> 8
        msg[7] = sz & 0x00ff

        self.sock.write(premsg)
        self.sock.write(msg)
        self.sock.write(self.client_id)

        if self.lw_topic:
            self._send_str(self.lw_topic)
            self._send_str(self.lw_msg)

        if self.user is not None:
            self._send_str(self.user)
            self._send_str(self.pswd)

        resp = self._read(4)
        if resp[0] != 0x20 or resp[1] != 0x02:
            raise MQTTException("invalid CONNACK")
        if resp[3] != 0:
            raise MQTTException(resp[3])
        return resp[2] & 1

    def disconnect(self):
        if self.sock is not None:
            try:
                self.sock.write(b"\xe0\0")
                self.sock.close()
            except:
                pass
            self.sock = None

    def ping(self):
        self.sock.setblocking(True)
        self.sock.write(b"\xc0\0")

    def publish(self, topic, msg, retain=False, qos=0):
        self.sock.setblocking(True)
        pkt = bytearray(b"\x30\0\0\0")
        pkt[0] |= qos << 1 | retain
        sz = 2 + len(topic) + len(msg)
        if qos > 0:
            sz += 2
        assert sz < 2097152
        i = 1
        while sz > 0x7f:
            pkt[i] = (sz & 0x7f) | 0x80
            sz >>= 7
            i += 1
        pkt[i] = sz
        self.sock.write(pkt, i + 1)
        self._send_str(topic)
        if qos > 0:
            self.pid += 1
            struct.pack_into("!H", pkt, 0, self.pid)
            self.sock.write(pkt, 2)
        self.sock.write(msg)
        if qos == 1:
            while 1:
                op = self.wait_msg()
                if op == 0x40:
                    sz = self._read(1)
                    assert sz == b"\x02"
                    rcv_pid = self._read(2)
                    rcv_pid = rcv_pid[0] << 8 | rcv_pid[1]
                    if self.pid == rcv_pid:
                        return
        elif qos == 2:
            assert 0

    def subscribe(self, topic, qos=0):
        assert self.cb is not None, "Subscribe callback is not set"
        pkt = bytearray(b"\x82\0\0\0")
        self.pid += 1
        struct.pack_into("!BH", pkt, 1, 2 + 2 + len(topic) + 1, self.pid)
        self.sock.write(pkt)
        self._send_str(topic)
        self.sock.write(qos.to_bytes(1, "little"))
        while 1:
            op = self.wait_msg()
            if op == 0x90:
                resp = self._read(4)
                assert resp[1] == pkt[2] and resp[2] == pkt[3]
                if resp[3] == 0x80:
                    raise MQTTException(resp[3])
                return

    def wait_msg(self):
        res = self.sock.read(1)
        if res is None:
            return None
        self.sock.setblocking(True)
        if res == b"":
            raise OSError(-1)
        if res == b"\xd0":
            sz = self._read(1)[0]
            assert sz == 0
            return None
        op = res[0]
        if op & 0xf0 != 0x30:
            return op
        sz = self._recv_len()
        topic_len = self._read(2)
        topic_len = (topic_len[0] << 8) | topic_len[1]
        topic = self._read(topic_len)
        sz -= topic_len + 2
        if op & 6:
            pid = self._read(2)
            pid = pid[0] << 8 | pid[1]
            sz -= 2
        msg = self._read(sz)
        self.cb(topic, msg)
        if op & 6 == 2:
            pkt = bytearray(b"\x40\x02\0\0")
            struct.pack_into("!H", pkt, 2, pid)
            self.sock.write(pkt)
        elif op & 6 == 4:
            assert 0
        return op

    def check_msg(self):
        self.sock.setblocking(False)
        return self.wait_msg()
