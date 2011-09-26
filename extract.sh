#!/bin/bash
stock=183828

grep analyze transactions/$stock-$(date +%F).txt | grep -o '[0-9]\+\.[0-9]\+' > /tmp/prices.dat

grep -o '{[0-9, .]\+}' transactions/183828-2011-09-21.txt \
    > /tmp/indicators.all

grep -o '{[0-9, .]\+}' /tmp/indicators.all | sed -n '1~3p' \
    | grep -v '{0' | sed -e 's/{1, \([0-9.]\+\), \([0-9.]\+\), \([0-9.]\+\)}/\1    \2    \3/g' \
    > /tmp/indicators.10min

grep -o '{[0-9, .]\+}' /tmp/indicators.all | sed -n '2~3p' \
    | grep -v '{0' | sed -e 's/{1, \([0-9.]\+\), \([0-9.]\+\), \([0-9.]\+\)}/\1    \2    \3/g' \
    > /tmp/indicators.2hour

grep -o '{[0-9, .]\+}' /tmp/indicators.all | sed -n '3~3p' \
    | grep -v '{0' | sed -e 's/{1, \([0-9.]\+\), \([0-9.]\+\), \([0-9.]\+\)}/\1    \2    \3/g' \
    > /tmp/indicators.day





