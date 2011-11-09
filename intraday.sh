#!/bin/bash

stock=183828

# 0: buy-and-sell
# 1: sell-and-buy
mode=1

# 0: complete
# 1: incomplete
status=0

# entering price
# ignored when status = 0
price=99.7

# time when the current incomplete position
# is entered.
enter_time="06:00:00"

# entering quantity
quantity=830

wd=/home/xxie/work/avanza/data_extract/intraday

function start_trading {
    ./intraday $stock $mode $status $price $enter_time$ $quantity
}

if [ "$wd" != `pwd` ]; then cd $wd; fi

for dir in transactions records logs positions; do
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
	if [ -z "`ps -C alarm.sh -o pid=`" ]; then
	    ./alarm.sh &
	fi
	start_trading
	;;
stop)
	killall intraday &> /tmp/null
	killall alarm.sh &> /tmp/null
	;;
restart)
	killall intraday &> /tmp/null
	if [ -z "`ps -C alarm.sh -o pid=`" ]; then
	    ./alarm.sh &
	fi
	start_trading
	;;
*)
	echo "Unknown command \"$command\""
	exit 1
esac

