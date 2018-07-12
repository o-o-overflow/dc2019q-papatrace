#!/usr/bin/env python2

from pwn import *
import sys

payload = """
3
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
    s = r.recvall()
    assert "Nope" in s

if __name__ == '__main__':
    main()
    

