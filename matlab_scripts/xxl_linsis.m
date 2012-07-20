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

persistent max_alpha;
persistent pillar_idx;
persistent saving_meal;

if isempty(saving_meal)
    saving_meal = 0;
end

stmt = sprintf(['select price from %s where tid like "%s ' ...
                '%%" and time(tid) <= "%s" order by tid asc;'], ...
               tbl_name, today, nowtime);
data = fetch(mysql, stmt);
price = cell2mat(data(:, 1));

N = length(price);
good_rate = 3.0e-3;
        
retcode = int8(0);
action = int8(NONE);
msg = sprintf('[%s]\n', nowtime);

if mymode == 1
    msg = sprintf('[%s] Mode 1 is not supported.\n', nowtime);
    return;
end

if (N < 1000 && mystatus == 0 ||...
    N < 100 && mystatus == 1)
    msg = sprintf('[%s] Only %d trades.\n',...
                  nowtime, length(price));
    return;
end

M = min([N, 2000]);
[alpha, S, mu] = polyfit((N-M+1:N)', price(N-M+1:N), 1);
if isempty(max_alpha) || alpha(1) > max_alpha
    max_alpha = alpha(1);
    pillar_idx = N;
end

n = 40;
[beta, S, mu] = polyfit((1:n)', price(N-n+1:N), 1);
beta_t = 2.0e-2;

n = min([N, 400]);
pivotal = 0;
[gamma, S, mu] = polyfit((1:n)', price(N-n+1:N), 3);
gamma_d = polyder(gamma);
gamma_dv = polyval(gamma_d, n, [], mu);
gamma_dvt = 6.0e-2;
% rollback from the scaling and centering transformation
gamma_d_roots = roots(gamma_d);
if isreal(gamma_d_roots)
    gamma_d_roots = gamma_d_roots*std(1:n) + mean(1:n);
    A = gamma_d_roots > 1 & gamma_d_roots < n;    
    if sum(A) == 2
        x = round(max(gamma_d_roots));
    elseif sum(A) == 1
        if A(1) == 1
            x = round(gamma_d_roots(1));
        else
            x = round(gamma_d_roots(2));
        end
    end
    if sum(A) >= 1
        flag = gamma_dv > 0;
        % flag = flag && price(N-n+x) < price(N-n+1);
        % flag = flag && price(N-n+x) < price(N);
        flag = flag && price(N) < price(N-n+x)*(1 + 1.5e-3);
        if flag
            pivotal = -1;
        end

        flag = gamma_dv < 0;
        % flag = flag && price(N-n+x) > price(N-n+1);
        % flag = flag && price(N-n+x) > price(N);
        flag = flag && price(N) > price(N-n+x)*(1 - 1.5e-3);
        if flag
            pivotal = 1;
        end
    end
end

if gamma_dv > gamma_dvt && beta(1) > -beta_t
    trend = 1;
elseif gamma_dv < -gamma_dvt && beta(1) < beta_t
    trend = -1;
else
    trend = 0;
end

% shield = update_shield4(mymode, beta(1), mysql, tbl_name, shield);

if mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity, price(end));
    if profit > myprice*myquantity*good_rate && trend == -1

        action = xxl_act(mymode, mystatus);
        saving_meal = 0;
        msg = sprintf(...
            ['[%s] price=%f; N=%d; profit=%f; gamma=%f'], nowtime, ...
            price(end), N, profit, gamma_dv);
        return;
    end
end

M = min(1000, N);
[zeta, S, mu] = polyfit((N-M+1:N)', price(N-M+1:N), 4);
zeta_d = polyder(zeta);
zeta_dv = polyval(zeta_d, N, [], mu);

abslevel = sum(price < price(end))/N;
minp = min(price);
if mystatus == 0

    if alpha(1) > 2.5e-1 && price(N) < minp * 1.01
        flag = trend == 1;
        msg = sprintf(['[%s] price=%.2f; N=%d; alpha(1)=%e; '...
                       'trend=[%e %e]; abslevel=%e'], nowtime, ...
                      price(end), N, alpha(1), gamma_dv, zeta_dv, ...
                      abslevel);
    
    elseif max_alpha > 0.1 && alpha(1) > 0.05 && alpha(1) < 0.3
        flag = pivotal == -1;
        flag = flag && alpha(1) > max_alpha*0.5;
        flag = flag && trend == 1;
        flag = flag && zeta_dv > 0;
        flag = flag && price(N) < minp * 1.02;
        flag = flag && datenum(nowtime) < datenum('16:30');

        % flag = flag && zeta_dv > 0.1;
        msg = sprintf(['[%s] price=%.2f; N=%d; max_alpha=%e; '...
                       'trend=[%e %e %e]; abslevel=%e'], nowtime,...
                      price(end), N, max_alpha, alpha(1), gamma_dv,...
                      zeta_dv, abslevel);
    
    elseif price(N) < price(1) * (1 - 2.6e-2)
        flag = pivotal == -1;
        flag = flag && trend == 1;
        flag = flag && price(N) - min(price)
        flag = flag && zeta_dv > 0;
        flag = flag && datenum(nowtime) < datenum('16:00');
        saving_meal = 1;
        msg = sprintf(['[%s] price=%.2f; N=%d; price(1)=%f; '...
                       'trend=[%e %e %e]'], nowtime, ...
                      price(end), N, price(1), ...
                      alpha(1), gamma_dv, zeta_dv);

    % elseif alpha(1) > 0 && abslevel < 0.4 && alpha(1) > 1.0e-2
    %     flag = flag && datenum(nowtime) < datenum('16:30');
    %     % flag = flag && alpha(1) > max_alpha*0.5;
    %     msg = sprintf(['[%s] price=%.2f; N=%d; max_alpha=%e; '...
    %                    'trend=[%e %e]; abslevel=%e'], nowtime, ...
    %                   price(end), N, max_alpha, alpha(1), gamma_dv);
    
    else
        flag = 0;
        msg = sprintf(['[%s] price=%.2f; N=%d; ' ...
                       'abslevel=%f; trend=[%e %e %e]'], ...
                      nowtime, price(end), N, ...
                      abslevel, alpha(1), gamma_dv, zeta_dv);
    end
    if (flag)
        action = xxl_act(mymode, mystatus);
    end
elseif mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity, price(end));
    % M = min([N, 2000]);
    % alpha_v = polyval(alpha, (N-M+1:N)', [], mu);
    % errors = abs(alpha_v - price(N-M+1:N));

    % plevel = sign(price(end) - alpha_v(end))*...
    %          sum(errors < abs(alpha_v(end) - price(end)))/N;
    % threshold = 0.825;
    flag = profit > 0;
    flag = flag && ~saving_meal;
    flag = flag && pivotal == 1;
    flag = flag && alpha(1) < 0;
    flag = flag && zeta_dv < 0;
    % flag = flag && (alpha(1) < 0.1 && plevel > threshold || ...
    %                 gamma_dv < -0.2);
    % flag = flag && (alpha(1) < 0.1 && plevel > threshold);
    if flag
        action = xxl_act(mymode, mystatus);
    end
    msg = sprintf(['[%s] price=%.2f; N=%d; ' ...
                   'profit=%f; abslevel=%e; '...
                   'trend=[%e %e %e]; Escape'], nowtime, ...
                  price(end), N, profit, abslevel, ...
                  alpha(1), gamma_dv, zeta_dv);
end
