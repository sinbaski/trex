#!/bin/bash

# for name in Alliance_Oil_Company_SDB Ericsson_B Nordea_Bank Securitas_B; do
#for name in Nordea_Bank Ericsson_B Volvo_B Trelleborg_B Hennes TeliaSonera; do
for name in Nordea_Bank; do
#for name in Nordea_Bank Ericsson_B Boliden TeliaSonera Trelleborg_B AstraZeneca; do
#for name in Ericsson_B Boliden TeliaSonera Trelleborg_B AstraZeneca; do
    grep -v ".\+#$" $name-days.txt | cut -f 1 | ./simulate_many.sh $name
    if [ $? -ne 0 ]; then
	exit $?
    fi
done
