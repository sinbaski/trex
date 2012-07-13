function [action, retcode, msg] = xxl_linsis9(my_position, dbinfo)

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

persistent shield;
persistent enable_shield;

if isempty(shield)
    shield = 0;
end

if isempty(enable_shield)
    stmt = sprintf(['select enable_shield from parameters '...
                    'where co_name ="%s";'], tbl_name);
    data = fetch(mysql, stmt);
    enable_shield = cell2mat(data(1));
end

stmt = sprintf(['select price from %s where tid like "%s ' ...
                '%%" and time(tid) <= "%s" order by tid asc;'], ...
               tbl_name, today, nowtime);
data = fetch(mysql, stmt);
price = cell2mat(data(:, 1));

N = length(price);
good_rate = 2.90e-3;
        
retcode = int8(0);
action = int8(NONE);

if (N < 400 && mystatus == 0 ||...
    N < 100 && mystatus == 1)
    msg = sprintf('[%s] Only %d trades.\n',...
                  nowtime, length(price));
    return;
end

[alpha, S, mu] = polyfit((1:N)', price, 1);
alpha_v = polyval(alpha, (1:N)', [], mu);
errors = abs(alpha_v - price);

M = min([N, 800]);
if M == N
    beta = alpha;
else
    [beta, S, mu] = polyfit((1:M)', price(end-M+1:end), 1);
end

n = min([N, 400]);
[gamma, S, mu] = polyfit((N-n+1:N)', price(N-n+1:N), 3);
gamma_d = polyder(gamma);
gamma_dv = polyval(gamma_d, N, [], mu);
gamma_dvt = 6.0e-2;

if gamma_dv > gamma_dvt
    trend = 1;
elseif gamma_dv < -gamma_dvt
    trend = -1;
else
    trend = 0;
end
shield = update_shield4(mymode, beta(1), mysql, tbl_name, shield);

if mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity, price(end));
    if (profit > myprice*myquantity*good_rate && ...
        (mymode == 0 && trend == -1 || mymode == 1 && trend == 1))

        action = xxl_act(mymode, mystatus);
        msg = sprintf(...
            ['[%s] price=%f; profit=%f; gamma=%f'], nowtime, ...
            price(end), profit, gamma_dv);
        return;
    end
end

n = day(today, 'yyyy-mm-dd');
if mystatus == 0 && (enable_shield || n <= 7 || n >= 20) && shield
    msg = sprintf(['[%s] price=%f; N=%d; beta(1)=%f; shield=%d'],...
                  nowtime, price(end), N, beta(1), shield);
    return;
end
threshold = 0.825;
clear n;

if (mystatus == 0 && N < 1000)
        msg = sprintf('[%s] Only %d trades.\n',...
                  nowtime, length(price));
        return;
end

plevel = sign(price(end) - alpha_v(end))*...
    sum(errors < abs(alpha_v(end) - price(end)))/N;
if mystatus == 0
    mindiff = xxl_mindiff(my_position, price(end));
    flag = 1;
    if (mymode == 0)
        % flag = flag && (price(end) < alpha_v(end) - mindiff*0.75 && ...
        %                 plevel < -threshold);

        flag = flag && plevel < -threshold;
        flag = flag && alpha(1) > -1.0e-1;
        flag = flag && trend == 1;
        flag = flag && ~(alpha(1) > 2.0e-1 && beta(1) < -3.20e-2);
    else
        % flag = flag && (price(end) > alpha_v(end) + mindiff*0.75 && ...
        %                 plevel > threshold);

        flag = flag && plevel > threshold;
        flag = flag && alpha(1) < 1.0e-1;
        flag = flag && trend == -1;
        flag = flag && ~(alpha(1) < -2.0e-1 && beta(1) > 3.20e-2);
    end
    if (flag)
        action = xxl_act(mymode, mystatus);
    end
    msg = sprintf(['[%s] price=%.2f + %.2f; Y=%.2f; N=%d; ' ...
                   'plevel=%f; lim=[%f %f]; trend=[%e %e %e]'], ...
                  nowtime, ...
                  price(end), mindiff, alpha_v(end), N, ...
                  plevel, alpha_v(end) - mindiff*0.75, ...
                  alpha_v(end) + mindiff*0.75, alpha(1), beta(1), ...
                  gamma_dv);
elseif mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity, price(end));
    abslevel = sum(price < price(end))/N;
    flag = profit > 0;
    if (mymode == 0)
        flag = flag && (plevel > threshold || abslevel > 0.9);
        flag = flag && trend == -1;
    else
        flag = flag && (plevel < -threshold || abslevel < 0.1);
        flag = flag && trend == 1;
    end
    if flag
        action = xxl_act(mymode, mystatus);
    end
    msg = sprintf(...
            ['[%s] price=%.2f; Y=%.2f; N=%d; ' ...
             'profit=%f; plevel=%f; gamma_dv=%e'], ...
        nowtime, price(end), alpha_v(end), N, profit, plevel, gamma_dv);
end
