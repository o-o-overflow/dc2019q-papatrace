#!/usr/bin/env python2

from pwn import *
import sys

payload = """
2
1
3
20
0
1
10000
"""

def main():

    host = sys.argv[1]
    port = int(sys.argv[2])

    r = remote(host, port)
    r.send(payload)
    assert "OOO is unwilling" in r.recvall()

if __name__ == '__main__':
    main()
    

