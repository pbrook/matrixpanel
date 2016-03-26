#! /usr/bin/env python3
import random
from rec import SwhRecorder
from matrixscreen import MatrixScreen

class game:
    running = True
    width = 640
    height = 640
    pixels = [0] * 4096
    pixels2 = [0] * 4096
    pixels3 = [0] * 4096
    offset = 0
    colours = [None] * 256

def loop():
    BLACK = (0,0,0)
    WHITE = (255,255,255)
    
    ys=game.SR.fft(minHz=400, maxHz=2500)
    ys2=game.SR2.fft(minHz=400, maxHz=2500)
    ys3=game.SR3.fft(minHz=400, maxHz=2500)

    yscale = 10 # higher means more sensitive
    yoffs = 50 # noise floor. lower means display more low-level noise 
    
    for i in range(0,64):
        y = int((ys[i]-yoffs)*yscale)
        y = 0 if y<0 else 255 if y>255 else y
        game.pixels[i+(game.offset*64)] = y

    game.offset = (game.offset + 1) & 63
    for i in range(0,4095):
        off = (i+game.offset*64)&4095
        ll = game.pixels[off]
        #ll2 = game.pixels2[4095-off]
        col = game.colours[ll]
        game.matrix.screen[(i&63),(i>>6)] = int(col[0]) + (int(col[1])<<8) + (int(col[2])<<16)

    game.matrix.send()
    
def inter(x,y,t):
    return x + (y-x) * t

def inter3(x,y,t):
    return (inter(x[0],y[0],t),
            inter(x[1],y[1],t),
            inter(x[2],y[2],t))

def main():
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
#    for i in xrange(0,128):
#        game.colours[i] = (0,0,i*2)
#        game.colours[i+128] = (i*2, i*2, 255)

    for i in range(0,128):
        game.colours[i] = inter3(COL1,COL2,i/128.0)
        game.colours[i+128] = inter3(COL2,COL3,i/128.0)

    while game.running:
        loop()

    game.SR.continuousEnd()

#this calls the 'main' function when this script is executed
if __name__ == '__main__': main()
