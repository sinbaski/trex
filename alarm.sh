#!/bin/bash

#prog=intraday

dir=/home/xxie/work/avanza/data_extract/intraday
mode=1
while test 1; do
    while read x && test -n "$x"; do
	# succeeded to sell Boliden at 125.60 kronor.
	if [ `echo "$x" | wc -w` -ne 7 ]; then
	    echo "$x" | festival --tts
	    continue;
	fi
	status=`echo "$x" | cut -d " " -f 1`
	if [ status == "succeeded" ]; then
	    action=`echo "$x" | cut -d " " -f 3`;
	    name=`echo "$x" | cut -d " " -f 4`;
	    price=`echo "$x" | cut -d " " -f 6`;
	fi
	echo "Ladies and gentlemen, may I have your attention? I" \
	    "$x" | festival --tts
    done < $dir/beep
done

