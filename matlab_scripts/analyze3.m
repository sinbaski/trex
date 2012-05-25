function [action, retcode, msg] =...
        analyze3(my_position, dbinfo)
    
BUY = 0;
SELL = 1;
NONE = 2;

% N = windownum * windowsize;

policy = struct('min_open_profit', 140, 'min_close_profit', 140);
mindata = 3200;
mymode = my_position{1, 1};
mystatus = my_position{1, 2};
myprice = my_position{1, 3};
myquantity = my_position{1, 4};

today = dbinfo{1, 1};
nowtime = dbinfo{1, 2};
tbl_name = dbinfo{1, 3};
mysql = dbinfo{1, 4};

stmt = sprintf(['select count(*) from %s where tid like "%s ' ...
                '%%" and time(tid) <= "%s";'], ...
               tbl_name, today, nowtime);
data = fetch(mysql, stmt);
N = data{1, 1};
if N < mindata
    stmt = sprintf(['select price from %s where tid <= "%s %s" order ' ...
                    'by tid desc limit %d;'], tbl_name, today, ...
                   nowtime, mindata);
    data = fetch(mysql, stmt);
    data = flipud(data);
else
    stmt = sprintf(['select price from %s where tid like "%s ' ...
                    '%%" and time(tid) <= "%s" order by tid asc;'], ...
                   tbl_name, today, nowtime);
    data = fetch(mysql, stmt);
end    
price = cell2mat(data(:, 1));
mindiff = xxl_mindiff(my_position, price(end));

N = length(price);
[action, retcode, msg1, error] =...
    xxl_linsis(my_position, dbinfo, price, [], Inf);
msg = sprintf('[%s] %s', nowtime, msg1);
