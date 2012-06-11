#!/bin/bash

#stock=(183950 217041 183830 181870)
# sname=(Alliance_Oil_Company_SDB Ericsson_B Nordea_Bank Securitas_B)
# price=("0" "0" "0" "0")
# status=(0 0 0 0)
price=0;
status=0;

stock=$1

quantity=
if [ -f $stock.txt ]; then
    rm $stock.txt
fi

echo "select dataid, my_quantity from company where name=\"$stock\"" | \
    mysql --skip-column-names --user=sinbaski --password=q1w2e3r4 avanza > \
    /tmp/tmp.txt

dataid=`cat /tmp/tmp.txt | awk '{print $1}'`;
quantity=`cat /tmp/tmp.txt | awk '{print $2}'`;

if [ -f ./pot.txt ]; then
    mv ./pot.txt /tmp
fi
echo 200000 > pot.txt

if [ -f $stock.txt ]; then
    mv $stock.txt temp
fi
while read x && test -n "$x"; do
    echo "$stock-$x to be simulated."
    day=$x;
    echo -s $dataid -m 0 -t $status -p $price -q $quantity -w 1 -n 1 -d $day > /tmp/tmp.txt
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
mv /tmp/pot.txt .
