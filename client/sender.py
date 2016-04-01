#! /usr/bin/env python3

import time
import math
import numpy as np
from matrixscreen import MatrixScreen

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

def draw(screen):
    global offset
    sz = 64
    scale = 0.5 * math.pi * 2.0 / float(sz)
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
        start = time.time()
        while (time.time() - start) < 30:
            now = time.time()
            if now > next_tick:
                print(frames)
                frames = 0
                next_tick += 1.0
            frames += 1
            draw(m.screen)
            m.send()
    except:
        m.screen[:] = 0
        m.send()

if __name__ == "__main__":
    main()
