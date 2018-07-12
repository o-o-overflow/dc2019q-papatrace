#!/bin/bash -e

gcc -o brute brute.c -lcrypto -lpthread
scp brute $1:
ssh $1 -t ./brute
