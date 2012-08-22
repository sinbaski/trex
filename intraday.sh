#!/bin/bash

wd=/home/xxie/work/avanza/data_extract/intraday

function start_trading {
    do_trade=$2;
    allow_new=$3;
    ./intraday -s $1 -w $do_trade -n $allow_new -d `date +%F` &
    rm /tmp/intraday-$1.txt
}

function stop_trading {
    pid=`ps -C intraday -o pid=,cmd= | grep $1 | grep -o '^ *[0-9]\+'`
    if [ -z "$pid" ]; then
	echo "None is running."
	return 0
    fi
    kill $pid
    # wait until the process is killed
    while ps -C intraday -o pid= | grep -q $pid; do
	sleep 3
    done
    return 0
}

if [ "$wd" != `pwd` ]; then cd $wd; fi

for dir in transactions logs; do
    if [ ! -d $dir ]; then
	mkdir $dir
    fi
done

if [ $# -lt 1 ]; then
    echo Usage: $0 start/stop [stock_id]
    exit 0
fi

command="$1"
case $command in
start)
	if [ $# -lt 4 ]; then
	    echo Only $# arguments while 4 is needed.
	    exit 1
	fi

	start_trading $2 $3 $4
	;;
stop)
	if [ $# -eq 2 ]; then
	    stop_trading $2
	elif [ $# -eq 1 ]; then
	    while read line; do
		if [ -z "$line" ] || [ ${line:0:1} == "#" ]; then
		    continue;
		fi
		stock=`echo "$line" | awk '{print $1}'`;
		stop_trading $stock
	    done < stocks.conf
	fi
	;;
*)
	echo "Unknown command \"$command\""
	exit -2
esac

