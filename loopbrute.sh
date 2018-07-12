#!/bin/bash

while :;
do
	./dobrute.sh $1 | tee /dev/stderr | grep --line-buffered COLLISION >> collisions
done
