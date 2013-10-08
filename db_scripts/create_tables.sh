# This is meant to create tables.


#create table traders (user varchar(16) not null, password varchar(32) not null);

for stock in 'Nordea_Bank' 'Ericsson_B' 'Boliden' 'ABB_Ltd' 'Hennes' 'Volvo_B'; do
    st="create table $stock (buyer varchar(16) default null, seller varchar(16) default null,";
    st="$st price double(7,2) unsigned not null, volume int(10) unsigned not null, tid datetime not null);"

    #st="alter table $stock change latest price double(7,2) unsigned not null";
    echo $st | mysql --skip-column-names --user=root --password=q1w2e3r4 avanza
done

