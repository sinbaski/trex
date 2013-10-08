#!/usr/bin/python

# load data
import os, re, _mysql, sys

# class Aktie:
#     latest = 0;
#     def __init__(self, name, code):
#         self.name = name;
#         self.code = code;

# stocks = [Aktie("boliden", "183828"),
#           Aktie('Securitas_B', '183950'),
#           Aktie('Alliance_Oil_Company_SDB', '217041'),
#           Aktie('Nordea_Bank', '183830'),
#           Aktie('Ericsson_B', '181870')]


os.chdir("/home/xxie/trade/intraday");
db = _mysql.connect("localhost", "sinbaski", "q1w2e3r4", "avanza")

# print sys.argv[1]
# print sys.argv[2]
fl = open(sys.argv[1], 'r')
for line in fl:
    #print line
    pattern = re.compile("([0-9]{2}:[0-9]{2}:[0-9]{2})[ \t]*([0-9]+[.][0-9]{2})[ \t]*([0-9]+)")
    result = pattern.findall(line);
    
    st = "insert into " + sys.argv[2] + " values (" + \
         result[0][1] + ", " + result[0][2] + ", '" + sys.argv[3] + " " + result[0][0] + "');"
    db.query(st);
    #print st
    
fl.close();
db.close()

