#!/bin/bash

wd=/home/xxie/work/avanza/data_extract/intraday

function start_trading {
    echo select my_mode, my_status, my_price, my_quantity \
	from company where dataid=$1 | mysql --skip-column-names \
	--user=sinbaski --password=q1w2e3r4 avanza > \
	/tmp/intraday-$1.txt
    if [ ! -s /tmp/intraday-$1.txt ]; then
	echo Unknown stock $1
	exit -1
    fi
    mode=`cut -f 1 /tmp/intraday-$1.txt`;
    status=`cut -f 2 /tmp/intraday-$1.txt`;
    price=`cut -f 3 /tmp/intraday-$1.txt`;
    quantity=`cut -f 4 /tmp/intraday-$1.txt`;
    # do_trade=`cut -f 5 /tmp/intraday-$1.txt`;
    do_trade=$2;
    allow_new=$3;
    ./intraday -s $1 -m $mode -t $status \
    	-q $quantity -p $price -w $do_trade \
    	-n $allow_new -d `date +%F` &
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
    #update the status of trading
    fname=`date +%F`
    fname="transactions/$1-$fname.txt"
    last_line=`grep executed $fname | tail -n 1`;
    if [ -z "$last_line" ]; then
	return 0;
    fi
    action=`echo "$last_line" | grep -o -E "BUY|SELL"`;
    price=`echo "$last_line" | sed -e "s/.*price=\([0-9.]\+\).*/\1/g"`;
    mode=`echo select my_mode from company where dataid=$1 | \
	mysql --skip-column-names --user=sinbaski --password=q1w2e3r4 avanza`;
    if [ "$mode" = "0" -a "$action" = "SELL" -o \
	"$mode" = "1" -a "$action" = "BUY" ]; then
	echo "update company set my_status=0, my_price=0 where dataid=$1" | \
	    mysql --skip-column-names --user=sinbaski --password=q1w2e3r4 avanza
    else
	echo "update company set my_status=1, my_price=$price where dataid=$1" | \
	    mysql --skip-column-names --user=sinbaski --password=q1w2e3r4 avanza
    fi
    return 0
}

if [ "$wd" != `pwd` ]; then cd $wd; fi

for dir in transactions logs; do
    if [ ! -d $dir ]; then
	mkdir $dir
    fi
done

if [ $# -lt 4 ]; then
    echo Only $# arguments while 4 is needed.
    exit 1
fi

# start_trading
command="$1"
case $command in
start)
	start_trading $2 $3 $4
	# if [ -n "$2" ]; then
	#     start_trading $2 $3 $4
	#     exit 0;
	# fi
	
	# while read stock && [ -n "$stock" ]; do
	#     if ps -C intraday -o cmd= | grep -q $stock; then
	# 	echo "$stock Already running."
	# 	continue;
	#     else
	# 	start_trading $stock
	#     fi
	# done < stocks.txt
	;;
stop)
	stop_trading $2
	# if [ -n "$2" ]; then
	#     stop_trading $2
	#     exit 0;
	# fi
	
	# while read stock && [ -n "$stock" ]; do
	#     stop_trading $stock
	# done < stocks.txt
	;;
*)
	echo "Unknown command \"$command\""
	exit -2
esac

