#!/bin/bash

stock=(183828 217041 183830 181870)
price=("0" "0" "0" "0")
status=(0 0 0 0)

function update {
    local i x p;
    for ((i=0; i<${#status[@]}; i++)); do
	x="$x ${status[$i]}"
	p="$p ${price[$i]}"
    done
    sed -i -e "s/status=(.*)/status=($x)/g" intraday.sh
    sed -i -e "s/price=(.*)/price=($p)/g" intraday.sh
    echo intraday.sh updated.
}

for ((i=0; i<${#stock[@]}; i++)); do
    if [ -f ${stock[$i]}.txt ]; then
	rm ${stock[$i]}.txt
    fi
    year=2012
    for ((month=1; month<=2; month++)); do
	for ((day=1; day<=31; day++)); do
	    x="`printf '%s-%02d-%02d' $year $month $day`"
	    echo ${stock[$i]}-$x to be simulated.

	    if [ -f records/${stock[$i]}-$x.dat -o -f temp/${stock[$i]}-$x.dat ]; then
		./simulate.sh sim ${stock[$i]} $x > /dev/null
		grep executed transactions/${stock[$i]}-`date +%F`.txt > /tmp/tmp.txt
		n=`wc -l /tmp/tmp.txt | grep -o '^[0-9]\+'`;
		if [ $n -eq 0 ]; then
		    continue;
		fi

		echo ====================${stock[$i]}-$x==================== >> ${stock[$i]}.txt
		cat /tmp/tmp.txt >> ${stock[$i]}.txt

		if [ $(($n % 2)) -eq 0 ]; then
		    continue;
		fi
	        # Negate the status
		s=${status[$i]};
		s=$((($s + 1) % 2));
		status[$i]=$s;
		# update the price
		price[$i]=`tail -n 1 /tmp/tmp.txt | sed -e "s/.*price=\([0-9.]\+\).*/\"\1\"/g"`;
		echo -n "status=$s "
		echo "price=${price[$i]}"
		update
	    fi
	done
    done
done
