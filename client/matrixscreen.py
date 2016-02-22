import socket
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
