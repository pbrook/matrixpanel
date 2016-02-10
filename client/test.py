#! /usr/bin/env python3

import socket
import time

COLUMNS = 32

TARGET_IP = "192.168.93.109"
TARGET_PORT = 3001

def main():
    s = socket.socket(type=socket.SOCK_DGRAM)
    data = bytearray(COLUMNS * 8)
    while True:
        s.sendto(data, (TARGET_IP, TARGET_PORT))
        time.sleep(1);


if __name__ == "__main__":
    main()
