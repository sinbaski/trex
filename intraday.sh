#!/bin/bash

stock=183828

# 0: buy-and-sell
# 1: sell-and-buy
mode=0

#entering price
price="88.50"

# entering quantity
quantity=730

trapped=0

wd=/home/xxie/work/avanza/data_extract/intraday

function start_trading {
    if [ "$trapped" == "0" ]; then
	./intraday -s $stock -m $mode -q $quantity -p $price
    else
	./intraday -s $stock -m $mode -q $quantity -p $price -t
    fi
}

function stop_trading {
    pid=`ps -C intraday -o pid=,cmd= | grep $stock | grep -o '^ *[0-9]\+'`
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

for dir in transactions records logs probdist; do
    if [ ! -d $dir ]; then
	mkdir $dir
    fi
done

if [ -z "$1" ]; then
    echo "Usage: $0 COMMAND" 1>&2
    exit 1
fi

# start_trading
command="$1"
shift

case $command in
start)
	if ps -C intraday -o cmd= | grep -q $stock; then
	    echo "Already running."
	    exit 0
	fi
	if [ `pgrep -c alarm.sh` -eq 0 ]; then
	    ./alarm.sh &
	fi
	start_trading
	;;
stop)
	stop_trading
	if [ `pgrep -c intraday` -eq 0 ]; then
	    killall alarm.sh &> /tmp/null
	fi
	;;
restart)
	stop_trading
	if [ `pgrep -c alarm.sh` -eq 0 ]; then
	    ./alarm.sh &
	fi
	start_trading
	;;
*)
	echo "Unknown command \"$command\""
	exit 1
esac

