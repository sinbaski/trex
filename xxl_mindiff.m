function mindiff = xxl_mindiff(my_position, price)

policy = struct('min_open_profit', 140, 'min_close_profit', 140);
mymode = my_position{1, 1};
mystatus = my_position{1, 2};
myprice = my_position{1, 3};
myquantity = my_position{1, 4};
feerate = 0.08e-2;

mindiff  = policy.min_open_profit + ...
    2 * price * feerate * myquantity;
mindiff = double(mindiff) / double(myquantity);
if mymode == 0
    mindiff = mindiff / (1 - feerate);
else
    mindiff = mindiff / (1 + feerate);
end
