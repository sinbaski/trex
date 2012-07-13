#!/bin/bash

# for co in Securitas_B Alliance_Oil_Company_SDB Nordea_Bank Ericsson_B; do
for co in Sandvik; do
#for co in Volvo_B Hennes; do
    echo "select distinct(date(tid)) from $co order by date(tid);" |
    mysql --skip-column-names --user=sinbaski --password=q1w2e3r4 avanza |
    while read line && test -n "$line"; do
	echo "select \"$line\", dayofweek(\"$line\")-1, time(min(tid)), time(max(tid)), count(*) from $co where tid like \"$line %\";"
    done | mysql --skip-column-names --user=sinbaski --password=q1w2e3r4 avanza > $co-days.txt
done



