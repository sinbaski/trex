function mindiff = xxl_mindiff(my_position, price, exit_rate)

%policy = struct('min_open_profit', 140, 'min_close_profit', 140);
mymode = my_position{1, 1};
mystatus = my_position{1, 2};
myprice = my_position{1, 3};
myquantity = my_position{1, 4};

mindiff = price * exit_rate + 99*2 / myquantity;

