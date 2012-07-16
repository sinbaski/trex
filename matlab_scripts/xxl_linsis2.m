function [action, retcode, msg] = xxl_linsis2(my_position, dbinfo)

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
good_rate = 2.90e-3;
        
retcode = int8(0);
action = int8(NONE);
msg = sprintf('[%s]\n', nowtime);

if mymode == 1
    msg = sprintf('[%s] Mode 1 is not supported.\n', nowtime);
    return;
end

if (N < 300 && mystatus == 0 ||...
    N < 100 && mystatus == 1)
    msg = sprintf('[%s] Only %d trades.\n',...
                  nowtime, length(price));
    return;
end

M = min([N, 2000]);
[alpha, S, mu] = polyfit((N-M+1:N)', price(N-M+1:N), 1);
alpha_v = polyval(alpha, (N-M+1:N)', [], mu);
errors = abs(alpha_v - price(N-M+1:N));
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
            ['[%s] price=%f; profit=%f; gamma=%f'], nowtime, ...
            price(end), profit, gamma_dv);
        return;
    end
end

M = min(1000, N);
[zeta, S, mu] = polyfit((N-M+1:N)', price(N-M+1:N), 4);
zeta_d = polyder(zeta);
zeta_dv = polyval(zeta_d, N, [], mu);

plevel = sign(price(end) - alpha_v(end))*...
         sum(errors < abs(alpha_v(end) - price(end)))/N;
threshold = 0.825;
abslevel = sum(price < price(end))/N;
if mystatus == 0
    flag = pivotal == -1;
    flag = flag && trend == 1;
    flag = flag && zeta_dv > 0;
    % if N < 800
    %     flag = flag && (alpha(1) > -0.1 && alpha(1) < 0.1 &&...
    %                     plevel < -0.9 || abs(alpha(1)) > 0.1); 
        
    %     flag = flag && alpha(1) < 0.1 && alpha(1) > -0.1;
    %     flag = flag && plevel < -threshold;
    %     msg = sprintf(['[%s] price=%.2f; N=%d; trend=[%e %e]'],...
    %                   nowtime, price(end), N, alpha(1), gamma_dv);

    if max_alpha > 0.1 && alpha(1) > 0.05
        flag = flag && datenum(nowtime) < datenum('16:30');
        flag = flag && alpha(1) > max_alpha*0.5;
        msg = sprintf(['[%s] price=%.2f; N=%d; max_alpha=%e; '...
                       'trend=[%e %e]'], nowtime, price(end), N,...
                      max_alpha, alpha(1), gamma_dv);
    
    elseif price(N) < price(1) * (1 - 2.6e-2)
        saving_meal = 1;
        msg = sprintf(['[%s] price=%.2f; N=%d; price(1)=%f; '...
                       'trend=[%e %e]'], nowtime, ...
                      price(end), N, price(1), ...
                      alpha(1), gamma_dv);

    elseif alpha(1) < 0.1 && alpha(1) > -4.0e-2
        mindiff = xxl_mindiff(my_position, price(end));        
        flag = flag && (price(end) < alpha_v(end) - mindiff*0.75);
        flag = flag && plevel < -threshold;
        flag = flag && abslevel > 0.1;
        msg = sprintf(['[%s] price=%.2f + %.2f; Y=%.2f; N=%d; ' ...
                       'plevel=%f; abslevel=%f; lim=[%f %f]; '...
                       'trend=[%e %e]'], ...
                      nowtime, ...
                      price(end), mindiff, alpha_v(end), N, ...
                      plevel, abslevel, alpha_v(end) - mindiff*0.75, ...
                      alpha_v(end) + mindiff*0.75, alpha(1), ...
                      gamma_dv);
    else
        flag = 0;
        msg = sprintf(['[%s] price=%.2f + %.2f; Y=%.2f; N=%d; ' ...
                       'plevel=%f; abslevel=%f; lim=[%f %f]; '...
                       'trend=[%e %e]'], ...
                      nowtime, ...
                      price(end), mindiff, alpha_v(end), N, ...
                      plevel, abslevel, alpha_v(end) - mindiff*0.75, ...
                      alpha_v(end) + mindiff*0.75, alpha(1), ...
                      gamma_dv);
    end
    if (flag)
        action = xxl_act(mymode, mystatus);
    end
elseif mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity, price(end));
    flag = profit > 0;
    flag = flag && ~saving_meal;
    flag = flag && trend == -1;
    % flag = flag && (alpha(1) < 0.1 && plevel > threshold || ...
    %                 gamma_dv < -0.2);
    flag = flag && (alpha(1) < 0.1 && plevel > threshold);
    if flag
        action = xxl_act(mymode, mystatus);
    end
    msg = sprintf(['[%s] price=%.2f; Y=%.2f; N=%d; ' ...
                   'profit=%f; abslevel=%e; plevel=%f; '...
                   'alpha(1)=%e; gamma_dv=%e'], nowtime, price(end),...
                  alpha_v(end), N, profit, abslevel, plevel,...
                  alpha(1), gamma_dv);
end
