#!/bin/bash

stock=183828

# 0: buy-and-sell
# 1: sell-and-buy
mode=1

# 0: complete
# 1: incomplete
status=0

# entering price
# ignored in the sell-and-buy mode
price=0

# entering quantity
# ignored in the buy-and-sell mode.
quantity=830

wd=/home/xxie/work/avanza/data_extract/intraday

if [ "$wd" != `pwd` ]; then cd $wd; fi
if [ ! -d transactions ]; then mkdir transactions; fi
if [ ! -d records ]; then mkdir records; fi
if [ ! -d logs ]; then mkdir logs; fi

function start_trading {
    ./intraday $stock $mode $status $price $quantity
}

# if [ -z "$1" ]; then
#     echo "Usage: $0 COMMAND" 1>&2
#     exit 1
# fi

start_trading
# command="$1"
# shift

# if ps -e | grep -q 'intraday.sh'; then
#     running=1
# fi

# case $command in
# status)
# 	if [ -n "$running" ]; then
# 	    echo "$0 is up and running"
# 	else
# 	    echo "$0 is not running"
# 	fi
# 	;;
# start)
# 	if [ -n "$running" ]; then
# 	    echo "$0 already up and running"
# 	    exit 0
# 	fi
# 	start_trading
# 	;;
# stop)
# 	if [ -n "$running" ]; then
# 	    killall intraday
# 	fi
# 	;;
# restart)
# 	if [ -n "$running" ]; then
# 	    killall intraday
# 	fi
# 	start_trading
# 	;;
# *)
# 	echo "Unknown command \"$command\""
# 	exit 1
# esac

