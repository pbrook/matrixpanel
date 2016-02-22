#! /usr/bin/env python3

import socket
import time
import math
import numpy as np

TARGET_IP = "192.168.11.2"
TARGET_PORT = 3001

class MatrixScreen():
    X = 64
    Y = 64

    ROWS = 8
    COLUMNS = (X * Y // (ROWS * 2))
    FRAGMENT_SIZE = 512
    FRAGMENTS_PER_ROW = (X * Y * 4) // (FRAGMENT_SIZE * ROWS)

    def __init__(self):
        self.screen = np.zeros((self.X, self.Y), dtype = np.uint32)
        self._socket = socket.socket(type=socket.SOCK_DGRAM)
        self._frame_num = 0

    def send(self):
        dest = (TARGET_IP, TARGET_PORT)
        n = 0
        for y0 in range(0, self.ROWS):
            for y1 in range(self.FRAGMENTS_PER_ROW):
                y = y0 + y1 * self.ROWS * 2
                header = bytes([3, self._frame_num, n, 0])
                buf = header + bytes(self.screen[:, y]) + bytes(self.screen[:, y + self.ROWS])
                self._socket.sendto(buf, dest);
                self._socket.recv(8)
                n += 1
        self._socket.sendto(bytes([1, self._frame_num, 0, 0]), dest)
        self._socket.recv(8)
        self._frame_num += 1
        if self._frame_num == 256:
            self._frame_num = 0

def pos_modf(val):
    val = math.modf(val)[0]
    if val < 0:
        return val + 1.0
    return val

def color_plasma(val):
    val = pos_modf(val) * 3.0
    if val < 1.0:
        r = val
        g = 1.0 - r
        b = 0.0
    elif val < 2.0:
        b = val - 1.0
        r = 1.0 - b
        g = 0.0
    else:
        g = val - 2.0
        b = 1.0 - g
        r = 0.0
    r = int(r * 256.0 - 0.5)
    g = int(g * 256.0 - 0.5)
    b = int(b * 256.0 - 0.5)
    return r | (g << 8) | (b << 16)

offset = 0.0
DT = 1/30

def tick(screen):
    global offset
    sz = 64
    scale = 1.5 * math.pi * 2.0 / float(sz)
    for y in range(0, sz):
        for x in range(0, sz):
            u = math.cos(x * scale)
            v = math.cos(y * scale)
            e = (u + v + 2.0) / 4.0
            color = color_plasma(offset + e)
            screen[x, y] = color
    offset += DT

def main():
    next_tick = time.time()
    frames = 0
    m = MatrixScreen()
    try:
        while True:
            now = time.time()
            if now > next_tick:
                print(frames)
                frames = 0
                next_tick += 1.0
            frames += 1
            tick(m.screen)
            m.send()
    except:
        m.screen[:] = 0
        m.send()

if __name__ == "__main__":
    main()
