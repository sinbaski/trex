#!/bin/bash

stock=(183828 217041 183830 181870)

# 0: buy-and-sell
# 1: sell-and-buy
mode=(0 0 0 0)

#entering price
price=("0" "0" "0" "0")

# entering quantity
quantity=(730 830 1358 1054)

# The data to use for calibration
calibration=(2012-03-19 2012-03-19 2012-03-19 2012-03-19)

status=(0 0 0 0)

wd=/home/xxie/work/avanza/data_extract/intraday

# function start_trading {
#     ./intraday -s $1 -m $2 -t $3 -q $4 -p $5 -c $6 -t $7
# }

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

for dir in transactions records logs; do
    if [ ! -d $dir ]; then
	mkdir $dir
    fi
done

if [ -z "$1" ]; then
    echo "Usage: $0 COMMAND" 1>&2
    exit -1
fi

# start_trading
command="$1"

case $command in
start)
	if [ -n "$2" ]; then
	    for ((i=0; i<${#stock[@]}; i++)); do
		if [ "${stock[$i]}" != "$2" ]; then
		    continue;
		fi
		./intraday -s ${stock[$i]} -m ${mode[$i]} -t ${status[$i]} \
		    -q ${quantity[$i]} -p ${price[$i]} -c ${calibration[$i]} &
		break;
	    done
	    exit 0;
	fi
	
	for ((i=0; i<${#stock[@]}; i++)); do
	    if ps -C intraday -o cmd= | grep -q ${stock[$i]}; then
		echo "${stock[$i]} Already running."
		continue;
	    else
		./intraday -s ${stock[$i]} -m ${mode[$i]} -t ${status[$i]} \
		    -q ${quantity[$i]} -p ${price[$i]} -c ${calibration[$i]} &
	    fi
	done
	if [ `pgrep -c alarm.sh` -eq 0 ]; then
	    ./alarm.sh &
	fi
	;;
stop)
	if [ -n "$2" ]; then
	    for ((i=0; i<${#stock[@]}; i++)); do
		if [ "${stock[$i]}" != "$2" ]; then
		    continue;
		fi
		stop_trading ${stock[$i]}
		break;
	    done
	    exit 0;
	fi
	
	for ((i=0; i<${#stock[@]}; i++)); do
	    stop_trading ${stock[$i]}
	done
	if [ `pgrep -c intraday` -eq 0 ]; then
	    killall alarm.sh &> /tmp/null
	fi
	;;
*)
	echo "Unknown command \"$command\""
	exit -2
esac

