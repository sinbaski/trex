# This is meant to create tables.

for stock in 'Securitas_B' 'Alliance_Oil_Company_SDB' 'Nordea_Bank' 'Ericsson_B'; do
    #st="create table $stock (latest double(7,2) unsigned not null, volume int(10) unsigned not null, tid datetime not null);"
    st="alter table $stock change latest price double(7,2) unsigned not null";
    echo $st | mysql --user=root --password=q1w2e3r4 avanza
done

