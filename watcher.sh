#!/bin/bash

wd=/home/xxie/work/avanza/data_extract/intraday

if [ "$wd" != `pwd` ]; then cd $wd; fi

if ! echo $PATH | grep -q MATLAB; then
    MATLAB_ROOT=/usr/local/MATLAB/R2010b
    PATH="$PATH:$MATLAB_ROOT/bin"
    export PATH
fi

while test 1; do
    while read line; do
	if [ -z "$line" ] || [ ${line:0:1} == "#" ]; then
	    continue;
	fi
	stock=`echo "$line" | awk '{print $1}'`;
	do_trade=`echo "$line" | awk '{print $2}'`;
	allow_new=`echo "$line" | awk '{print $3}'`;

	log="./logs/$stock-"
	log+=`date +%F`
	log+=".log"

	file="watcher-$stock"
	if [ ! -f $file ]; then
	    echo "[`date +%H:%M:%S`] starting $stock..."
	    ./intraday.sh start $stock $do_trade $allow_new
	elif ps -C intraday -o cmd= | grep -q $stock; then
	    x="`stat -c %Y $file`"
	    y="`date +%s`"
	    if [ $(($y - $x)) -gt 310 ]; then
		echo "[`date +%H:%M:%S`] restarting $stock"
		./intraday.sh stop $stock
		./intraday.sh start $stock $do_trade $allow_new
	    fi
	else
	    echo "[`date +%H:%M:%S`] starting $stock..."
	    ./intraday.sh start $stock $do_trade $allow_new
	fi
    done < stocks.conf
    sleep 310
done &
