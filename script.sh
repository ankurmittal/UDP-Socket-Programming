#!/bin/sh

for i in $(seq 1 10);
do
	echo $i
	./client > test
	diff server.c test
	echo "removing test"
	rm test
done
