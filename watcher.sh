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

stocks=(183950 217041 183830 181870)
while test 1; do
    for ((i=0; i<${#stocks[@]}; i++)); do
	stock=${stocks[$i]};
	echo "working on $stock."

	log="./logs/$stock-"
	log+=`date +%F`
	log+=".log"

	file="watcher-$stock"
	if [ ! -f $file ]; then
	    echo "[`date +%H:%M:%S`] starting $stock..."
	    ./intraday.sh start $stock `date +%F`
	elif ps -C intraday -o cmd= | grep -q $stock; then
	    x="`stat -c %Y $file`"
	    y="`date +%s`"
	    if [ $(($y - $x)) -gt 310 ]; then
		echo "[`date +%H:%M:%S`] restarting $stock"
		./intraday.sh stop $stock
		./intraday.sh start $stock `date +%F`
	    fi
	else
	    echo "[`date +%H:%M:%S`] starting $stock..."
	    ./intraday.sh start $stock `date +%F`
	fi
    done
    sleep 310
done &
