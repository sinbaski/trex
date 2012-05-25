function mindiff = xxl_mindiff(my_position, price)

%policy = struct('min_open_profit', 140, 'min_close_profit', 140);
mymode = my_position{1, 1};
mystatus = my_position{1, 2};
myprice = my_position{1, 3};
myquantity = my_position{1, 4};
feerate = 0.8e-3;
min_rate = 2.0e-3;

mindiff = price*(min_rate + 2*feerate);
%mindiff = double(mindiff) / double(myquantity);
if mymode == 0
    mindiff = mindiff / (1 - feerate);
else
    mindiff = mindiff / (1 + feerate);
end
