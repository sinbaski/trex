cd ~/trade/intraday

# tb_names=('Securitas_B' 'Alliance_Oil_Company_SDB' 'Nordea_Bank' 'Ericsson_B' 'boliden');
# ids=(183950 217041 183830 181870 183828);

tb_names=('Nordea_Bank' 'Ericsson_B' 'Boliden');
ids=(183830 181870 183828);

for ((i=0; i<${#tb_names[@]}; i++)); do
    stock=${tb_names[$i]};
    for f in `ls records/${ids[$i]}-*.dat`; do
	day=`basename $f | sed -e "s/${ids[$i]}-//g" | sed -e "s/\.dat//g"`
	db_scripts/load_data.py $f $stock $day 
	echo "done with $stock-$day"
	# echo "delete from $stock where tid like '$day %'; "

	# while read line && [ -n "$line" ]; do
	#     t=`echo $line | grep -o "[0-9]\{2\}:[0-9]\{2\}:[0-9]\{2\}"`
	#     price=`echo $line | grep -o "[0-9]\+\.[0-9]\{2\}"`
	#     volume=`echo $line | grep -o "[0-9]\+$"`
	#     st="insert into $stock values ($price, $volume, '$day $t');"
	#     echo "$st"
	# done < $f
    #done
    done
#done  | mysql --user=root --password=q1w2e3r4 avanza
done &> /tmp/log2.txt
