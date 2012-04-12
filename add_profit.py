#!/usr/bin/python
import os, re, _mysql, sys

os.chdir("/home/xxie/work/avanza/data_extract/intraday");

fl = open(sys.argv[1], 'r');
pattern = re.compile("profit=([0-9.]+)");
sum=0;
for line in fl:
    res = pattern.findall(line);
    if not res:
        continue
    sum += float(res[0])

print "The total profit is %f"%sum
