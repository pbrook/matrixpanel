#! /usr/bin/env python3
import random
from rec import SwhRecorder
from matrixscreen import MatrixScreen
import numpy as np
import time

class Game:
    def __init__(self):
        self.color1 = np.zeros((64, 64), dtype=np.uint32)
        self.color2 = np.zeros((64, 64), dtype=np.uint32)
        self.offset = 0
        self.colors = [None] * 256
        self.reds = [None] * 256
        self.blues = [None] * 256

    def loop(self):
        ys=game.SR.fft(minHz=400, maxHz=2500)
        ys2=game.SR2.fft(minHz=400, maxHz=2500)
        ys3=game.SR3.fft(minHz=400, maxHz=2500)

        yscale = 10 # higher means more sensitive
        yoffs = 50 # noise floor. lower means display more low-level noise

        row = np.zeros(64, dtype=np.uint32)

        for i in range(64):
            y = int((ys[i]-yoffs)*yscale)
            y = max(0, min(y, 255))
            row[i] = self.reds[y]
        self.color1 = np.roll(self.color1, 1, axis=1)
        self.color1[:, 0] = row[:]

        for i in range(64):
            y = int((ys2[i]-yoffs)*yscale)
            y = max(0, min(y, 255))
            row[i] = self.blues[y]
        self.color2 = np.roll(self.color2, 1, axis=0)
        self.color2[0, :] = row[:]

        gv = np.add(self.color1, self.color2)
        game.offset = (game.offset + 1) & 63
        for x in range(64):
            for y in range(64):
                ll = gv[x, y]
                game.matrix.screen[x, y] = ll

        game.matrix.send()
    
def inter(x,y,t):
    return x + (y-x) * t

def inter3(x,y,t):
    return (inter(x[0],y[0],t),
            inter(x[1],y[1],t),
            inter(x[2],y[2],t))

def color_int(t):
    return int(t[0]) + (int(t[1])<<8) + (int(t[2])<<16)

def main():
    global game

    game = Game()
    game.SR=SwhRecorder()
    game.SR.setup(3)
    game.SR.continuousStart()
    game.SR2=SwhRecorder()
    game.SR2.setup(4)
    game.SR2.continuousStart()
    game.SR3=SwhRecorder()
    game.SR3.setup(5)
    game.SR3.continuousStart()
    game.matrix = MatrixScreen()
    
    COL1 = (0,0,64)
    COL2 = (0,255,0)
    COL3 = (255,0,0)

    for i in range(0,128):
        game.colors[i] = color_int(inter3(COL1,COL2,i/128.0))
        game.colors[i+128] = color_int(inter3(COL2,COL3,i/128.0))

    BLACK = (0, 0, 0)
    RED = (128, 0, 0)
    BLUE = (0, 255, 0)
    for i in range(256):
        game.reds[i] = color_int(inter3(BLACK, RED, i / 256))
        game.blues[i] = color_int(inter3(BLACK, BLUE, i / 256))

    next_tick = time.time() + 1.0
    frames = 0;
    while True:
        game.loop()
        frames += 1
        if time.time() > next_tick:
            print(frames)
            next_tick += 1.0
            frames = 0

    game.SR.continuousEnd()

#this calls the 'main' function when this script is executed
if __name__ == '__main__': main()
