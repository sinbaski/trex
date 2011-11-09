#!/bin/bash

#prog=intraday

dir=/home/xxie/work/avanza/data_extract/intraday

while test 1; do
    while read x && test -n "$x"; do
	echo "$x" | festival --tts
    done < $dir/beep
done

