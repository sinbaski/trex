#!/bin/bash

wd=/home/xxie/work/avanza/data_extract/intraday

if [ "$wd" != `pwd` ]; then cd $wd; fi

while test 1; do
    stock=183828
    log="./logs/$stock-"
    log+=`date +%F`
    log+=".log"

    file="watcher-$stock"
    if [ ! -f $file ]; then
	echo "xxl_watcher: starting..." >> $log
	./intraday.sh start
    elif [ -n "`ps -C intraday -o cmd= | grep $stock`" ]; then
	x="`stat -c %Y $file`"
	y="`date +%s`"
	if [ $(($y - $x)) -gt 310 ]; then
	    echo "xxl_watcher: restarting..." >> $log
	    ./intraday.sh restart
	fi
    else
	echo "xxl_watcher: starting..." >> $log
	./intraday.sh start
    fi
    sleep 310
done &
