#!/bin/bash

while test 1; do
    stock=183828

    file="`date +%F`"
    file="./records/$stock-$file.dat"

    if [ ! -f $file ]; then
	./intraday.sh start
    elif [ -n "`ps -C intraday -o cmd= | grep $stock`" ]; then
	x="`stat -c %Y $file`"
	y="`date +%s`"
	if [ $(($y - $x)) -gt 310 ]; then
	    ./intraday.sh restart
	fi
    else
	./intraday.sh start
    fi
    sleep 310
done