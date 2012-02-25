#!/bin/bash

cmd=$1
stock=$2
today=`date +%F`

case $cmd in
clean)
	find . -name "$stock-$today.*" | grep -v temp | xargs rm
	;;

mv)
	find . -name "$stock-$today.*" | grep -v temp | xargs rm
	mv temp/$stock-*.txt transactions
	mv temp/$stock-*.dat records
	mv temp/$stock-*.log logs
	;;
sim)
	day=$3
	if [ ! -f temp/$stock-$day.dat ]; then
	    mv $(find . -name "$stock-$day.*") temp
	fi

	if [ ! -f temp/$stock-$today.dat -a -f records/$stock-$today.dat ]; then
	    mv $(find . -name "$stock-$today.*") temp
	elif [ -f records/$stock-$today.dat ]; then
	    find . -name "$stock-$today.*" | grep -v temp | xargs rm	    
	fi
	fakesource/fakesource temp/$stock-$day.dat $stock &
	./intraday.sh start $stock &
	while [ -n "`ps -C fakesource -o pid=`" ]; do
	    echo waiting for the fakesource to finish...
	    sleep 1
	done
	./intraday.sh stop $stock
	echo Simulation is complete.
esac