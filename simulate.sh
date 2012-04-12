#!/bin/bash

cmd=$1
sname=$2
stock=$3

case $cmd in
    clean)
	find . -name "$stock-$day.*" | grep -v temp | xargs rm
	;;

    mv)
	find . -name "$stock-$day.*" | grep -v temp | xargs rm
	mv temp/$stock-*.txt transactions
	mv temp/$stock-*.dat records
	mv temp/$stock-*.log logs
	;;
    sim)
	if [ -z "$4" ]; then
	    echo you also need to specify the date.
	    exit -1;
	fi
	day="$4"
	# if [ -f temp/$stock-$day.txt ]; then
	#     if [ `find . -name "$stock-$day.*" | grep -v temp | wc -l` -ne 0 ]; then
	# 	find . -name "$stock-$day.*" | grep -v temp | xargs rm
	#     fi
	# else
	#     mv $(find . -name "$stock-$day.*" | grep -v temp) temp
	# fi
	rm -f $(find . -name $stock-$day.*)
	if grep -q -e '-t 1 -p 0' /tmp/tmp.txt; then
	    echo -n "Incorrect options: "
	    cat /tmp/tmp.txt
	    exit -1;
	fi
	read opt < /tmp/tmp.txt 
	fakesource/fakesource $sname $day &
	echo "$opt"
	./intraday $opt
	# while [ -n "`ps -C fakesource -o pid=`" ]; do
	#     sleep 1
	# done
	# ./intraday.sh stop $stock
	echo Simulation is complete.
esac
