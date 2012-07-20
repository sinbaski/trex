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
	rm -f $(find . -name $stock-$day.*)
	if grep -q -e '-t 1 -p 0' /tmp/tmp.txt; then
	    echo -n "Incorrect options: "
	    cat /tmp/tmp.txt
	    exit -1;
	fi
	read opt < /tmp/tmp.txt 
	fakesource/fakesource $sname $day &
	echo "$opt"
	./intraday-test $opt
	echo Simulation is complete.
esac
