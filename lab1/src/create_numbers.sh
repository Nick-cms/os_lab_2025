#!/bin/bash

for i in {1..150}; do
	random_num=$(od -An -N2 -i /dev/random | head -n 1 | awk '{print $1 % 1001}')
	echo $random_num
done > numbers.txt
