% function [action, retcode, msg, error] = xxl_linsis(my_position, dbinfo...
%                                                     price, volume, ...
%                                                     allowed_error)
function [action, retcode, msg, error] =...
        xxl_linsis(my_position, dbinfo)

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

retcode = int8(0);
action = int8(NONE);

if (N < 400 && mystatus == 0 ||...
    N < 300 && mystatus == 1)
    msg = sprintf('[%s] Only %d trades.\n',...
                  nowtime, length(price));
    return;
end
    
index = (1:N)';
coeff = polyfit(index, price, 1);
Y = polyval(coeff, index);
errors = abs(Y - price);
sigma = std(Y - price);
error = sigma/mean(price);
% if error >= allowed_error
%     fprintf(2, 'error=%e > %e.\n', error, allowed_error);
%     return;
% end

% N = min([N, 500]);
plevel = sign(price(end) - Y(end))*...
    sum(errors < abs(Y(end) - price(end)))/N;
if mystatus == 0
    mindiff = xxl_mindiff(my_position, price(end));
    delta = sigma * 1.5;
    if (mindiff < delta &&...
        (mymode == 0 && plevel < -0.9 ||...
        mymode == 1 && plevel > 0.9))
        action = xxl_act(mymode, mystatus);
    end
    msg = sprintf(['[%s] price=%f + %f; %f=%e * %d + %f; error=%e; ' ...
                   'plevel=%f; delta=%f'],nowtime, ...
                  price(end), mindiff, Y(end), coeff(1), N,...
                  coeff(2), error, plevel, delta);
elseif mystatus == 1% && plevel > 0.9
    profit = xxl_profit(mymode, myprice, myquantity, price(end));
    if (profit < 0 )
        msg = sprintf(...
            ['[%s] price=%f; %f=%e * %d + %f; error=%e; ' ...
             'profit=%f; plevel=%f'], nowtime, ...
            price(end), Y(end), coeff(1), N, coeff(2), error, ...
            profit, plevel);
        return
    end
    if ((mymode == 0 && plevel > 0.9) ||...
        (mymode == 1 && plevel < -0.9))

        action = xxl_act(mymode, mystatus);
        msg = sprintf(...
            ['[%s] price=%f; %f=%e * %d + %f; error=%e; ' ...
             'profit=%f; plevel=%f'], nowtime, ...
            price(end), Y(end), coeff(1), N, coeff(2), error, ...
            profit, plevel);
    else
        good_rate = 2.9e-3;
        % stmt = sprintf(['select price from %s where tid like "%s %%" and ' ...
        %                 'time(tid) > "%s" and time(tid) <= "%s" ' ...
        %                 'order by tid asc;'], ...
        %                tbl_name, today, mytime, nowtime);
        stmt = sprintf(['select price from %s where tid <= "%s %s" ' ...
                        'order by tid desc limit 30;'], tbl_name, ...
                       today, nowtime);
        P = fetch(mysql, stmt);
        P = flipud(P);
        P = cell2mat(P(:, 1));
        % avgs = average_price(P, [], 15);
        avgs = [mean(P(1:15, 1)), mean(P(16:30, 1))];
        if (profit > myprice*myquantity*good_rate && ...
            (mymode == 0 && avgs(end) < avgs(end-1) || ...
             mymode == 1 && avgs(end) > avgs(end-1)))

            action = xxl_act(mymode, mystatus);            
            msg = sprintf(...
                ['[%s] price=%f; %f=%e * %d + %f; error=%e; ' ...
                 'profit=%f; level=%f; avgs=[%f %f]'], nowtime, ...
                price(end), Y(end), coeff(1), N, coeff(2), error, ...
                profit, plevel, avgs(end-1), avgs(end));
        end
    end
end
