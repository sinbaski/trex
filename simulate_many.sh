#!/bin/bash

#stock=(183950 217041 183830 181870)
# sname=(Alliance_Oil_Company_SDB Ericsson_B Nordea_Bank Securitas_B)
# price=("0" "0" "0" "0")
# status=(0 0 0 0)
price=0;
status=0;

# function update {
#     local i x p;
#     for ((i=0; i<${#status[@]}; i++)); do
# 	x="$x ${status[$i]}"
# 	p="$p ${price[$i]}"
#     done
#     sed -i -e "s/status=(.*)/status=($x)/g" intraday.sh
#     sed -i -e "s/price=(.*)/price=($p)/g" intraday.sh
#     echo intraday.sh updated.
# }

stock=$1
if [ -f $stock.txt ]; then
    rm $stock.txt
fi
echo "select dataid from company where name=\"$stock\"" |
mysql --skip-column-names --user=sinbaski --password=q1w2e3r4 \
    avanza > /tmp/tmp.txt
read dataid < /tmp/tmp.txt

while read x && test -n "$x"; do
    echo "$stock-$x to be simulated."
    day=$x;
    echo -s $dataid -m 0 -t $status -p $price -q 1200 -w 1 -d $day > /tmp/tmp.txt
    if ! ./simulate.sh sim $stock $dataid $day; then
	exit $?
    fi
    grep executed transactions/$dataid-$day.txt > /tmp/tmp.txt
    n=`wc -l /tmp/tmp.txt | grep -o '^[0-9]\+'`;
    if [ $n -eq 0 ]; then
	continue;
    fi

    echo ====================$stock-$x==================== >> $stock.txt
    cat /tmp/tmp.txt >> $stock.txt

    if [ $(($n % 2)) -ne 0 ]; then
	status=$((($status + 1) % 2));
    fi
    price=`tail -n 1 /tmp/tmp.txt | sed -e "s/.*price=\([0-9.]\+\).*/\1/g"`;
    echo -n "status=$status "
    echo "price=$price"
done
