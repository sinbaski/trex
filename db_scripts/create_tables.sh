#!/bin/bash
# This is meant to create tables.


#create table traders (user varchar(16) not null, password varchar(32) not null);

for stock in nordea_bank swedbank_a volvo_b scania_b boliden abb_ltd hennes___mauritz_b; do
    st="create table $stock (buyer varchar(16) default null, seller varchar(16) default null,";
    st="$st price decimal(7,2) unsigned not null, volume int unsigned not null, tid datetime not null);"

    # #st="alter table $stock change latest price double(7,2) unsigned not null";
    # st="drop table $stock";
    echo $st | mysql --skip-column-names --user=root --password=q1w2e3r4 avanza
done

