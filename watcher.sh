#!/bin/bash

wd=/home/xxie/work/avanza/data_extract/intraday

if [ "$wd" != `pwd` ]; then cd $wd; fi

if [ -z "$LD_LIBRARY_PATH" ]; then
    LD_LIBRARY_PATH=/usr/local/MATLAB/R2010b/bin/glnxa64
    LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/MATLAB/R2010b/sys/os/glnxa64
    export LD_LIBRARY_PATH

    MATLAB_ROOT=/usr/local/MATLAB/R2010b
    PATH="$PATH:$MATLAB_ROOT/bin"
    export PATH
fi

while test 1; do
    while read stock && [ -n "$stock" ]; do
	log="./logs/$stock-"
	log+=`date +%F`
	log+=".log"

	file="watcher-$stock"
	if [ ! -f $file ]; then
	    echo "[`date +%H:%M:%S`] starting $stock..."
	    ./intraday.sh start $stock
	elif ps -C intraday -o cmd= | grep -q $stock; then
	    x="`stat -c %Y $file`"
	    y="`date +%s`"
	    if [ $(($y - $x)) -gt 310 ]; then
		echo "[`date +%H:%M:%S`] restarting $stock"
		./intraday.sh stop $stock
		./intraday.sh start $stock
	    fi
	else
	    echo "[`date +%H:%M:%S`] starting $stock..."
	    ./intraday.sh start $stock
	fi
    done < stocks.txt
    sleep 310
done &
