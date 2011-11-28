#!/bin/bash

#prog=intraday

dir=/home/xxie/work/avanza/data_extract/intraday
mode=1
while test 1; do
    while read x && test -n "$x"; do
	# The order to %s %s at %s is %s.
	if [ `echo "$x" | wc -w` -ne 9 ]; then
	    echo "$x" | festival --tts
	    continue;
	fi
	status=`echo "$x" | cut -d " " -f 9`
	if [ status == "executed" ]; then
	    action=`echo "$x" | cut -d " " -f 4`;
	    name=`echo "$x" | cut -d " " -f 5`;
	    price=`echo "$x" | cut -d " " -f 7`;
	fi
	echo "Ladies and gentlemen, may I have your attention? " \
	    "$x" | festival --tts
    done < $dir/beep
done

