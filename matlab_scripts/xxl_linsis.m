function [action, retcode, msg] = xxl_linsis(my_position, dbinfo)

BUY = int8(0);
SELL = int8(1);
NONE = int8(2);
mindata = 100;

mymode = my_position{1, 1};
mystatus = my_position{1, 2};
myprice = my_position{1, 3};
myquantity = my_position{1, 4};
mytime = my_position{1, 5};

today = dbinfo{1, 1};
nowtime = dbinfo{1, 2};
tbl_name = dbinfo{1, 3};
mysql = dbinfo{1, 4};

stmt = sprintf(['select price from %s where tid like "%s ' ...
                '%%" and time(tid) <= "%s" order by tid asc;'], ...
               tbl_name, today, nowtime);
data = fetch(mysql, stmt);
price = cell2mat(data(:, 1));

N = length(price);
n = 40;
good_rate = 2.9e-3;
        
retcode = int8(0);
action = int8(NONE);

if (N < 400 && mystatus == 0 ||...
    N < n && mystatus == 1)
    msg = sprintf('[%s] Only %d trades.\n',...
                  nowtime, length(price));
    return;
end

trend = polyfit((1:n)', price(end-n+1:end), 1);
if mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity, price(end));
    if (profit > myprice*myquantity*good_rate && ...
        (mymode == 0 && trend(1) < 0 || ...
         mymode == 1 && trend(1) > 0))

        action = xxl_act(mymode, mystatus);
        msg = sprintf(...
            ['[%s] price=%f; profit=%f; trend=%f'], nowtime, ...
            price(end), profit, trend(1));
        return;
    end
end
    
index = (1:N)';
coeff = polyfit(index, price, 1);
Y = polyval(coeff, index);
errors = abs(Y - price);
% sigma = std(Y - price);
% error = sigma/mean(price);
plevel = sign(price(end) - Y(end))*...
    sum(errors < abs(Y(end) - price(end)))/N;

if mystatus == 0
    mindiff = xxl_mindiff(my_position, price(end));
    % delta = sigma * 1.5;
    if (mymode == 0 && price(end) > Y(end) - mindiff*0.75 ||...
        mymode == 1 && price(end) < Y(end) + mindiff*0.75)
        ;
    elseif (mymode == 0 && plevel < -0.9 && trend(1) > 0 ||...
        mymode == 1 && plevel > 0.9  && trend(1) < 0)
        action = xxl_act(mymode, mystatus);
    end
    msg = sprintf(['[%s] price=%f + %f; %f=%e * %d + %f; ' ...
                   'plevel=%f; lim=[%f %f]; trend=%f'], nowtime, ...
                  price(end), mindiff, Y(end), coeff(1), N,...
                  coeff(2), plevel, Y(end) - mindiff*0.75, ...
                  Y(end) + mindiff*0.75, trend(1));
elseif mystatus == 1% && plevel > 0.9
    profit = xxl_profit(mymode, myprice, myquantity, price(end));
    if (profit < 0 )
        msg = sprintf(...
            ['[%s] price=%f; %f=%e * %d + %f; ' ...
             'profit=%f; plevel=%f'], nowtime, ...
            price(end), Y(end), coeff(1), N, coeff(2), ...
            profit, plevel);
        return
    end
    if ((mymode == 0 && plevel > 0.9  && trend(1) < 0) ||...
        (mymode == 1 && plevel < -0.9 && trend(1) > 0))

        action = xxl_act(mymode, mystatus);
    end
    msg = sprintf(...
        ['[%s] price=%f; %f=%e * %d + %f; ' ...
         'profit=%f; plevel=%f; trend=%f'], nowtime, ...
        price(end), Y(end), coeff(1), N, coeff(2), ...
        profit, plevel, trend(1));
end
