#!/bin/bash

wd=/home/xxie/intraday

if [ "$wd" != "`pwd`" ]; then cd $wd; fi

# MATLAB_ROOT is NOT set because ~/.bashrc is not run when
# this script is run by crontab
export MATLAB_ROOT=/usr/local/MATLAB/R2013a
export PATH=$PATH:$MATLAB_ROOT/bin
# export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$MATLAB_ROOT/bin/glnxa64

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
	    echo "[`date`] starting $stock..." >> watcher.log
	    ./intraday.sh start $stock $do_trade $allow_new
	    sleep 10
	elif ps -C intraday -o cmd= | grep -q $stock; then
	    x="`stat -c %Y $file`"
	    y="`date +%s`"
	    if [ $(($y - $x)) -gt 310 ]; then
		echo "[`date`] restarting $stock" >> watcher.log
		./intraday.sh stop $stock
		./intraday.sh start $stock $do_trade $allow_new
	    fi
	else
	    echo "[`date`] starting $stock..." >> watcher.log
	    ./intraday.sh start $stock $do_trade $allow_new
	    sleep 10
	fi
    done < stocks.conf
    sleep 310
done &
