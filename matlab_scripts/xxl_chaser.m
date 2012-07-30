function [action, retcode, msg] = xxl_chaser(my_position, dbinfo)

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

if (N < 800 && mystatus == 0 ||...
    N < 100 && mystatus == 1)
    msg = sprintf('[%s] Only %d trades.\n',...
                  nowtime, length(price));
    return;
end

n = min([N, 400]);
pivotal = 0;
[gamma, S, mu] = polyfit((1:n)', price(N-n+1:N), 3);
gamma_d = polyder(gamma);
gamma_dv = polyval(gamma_d, n, [], mu);
gamma_dvt = 6.0e-2;
% rollback from the scaling and centering transformation
gamma_d_roots = roots(gamma_d);
gamma_d_roots =...
    gamma_d_roots(gamma_d_roots == real(gamma_d_roots));
gamma_d_roots = round(gamma_d_roots*std(1:n) + mean(1:n));
gamma_d_roots = unique(gamma_d_roots);
A = gamma_d_roots > 1 & gamma_d_roots < n;
gamma_d_roots = gamma_d_roots(A);
if length(gamma_d_roots) == 2
    x = round(max(gamma_d_roots));
elseif length(gamma_d_roots) == 1
    x = round(gamma_d_roots(1));
end
if ~isempty(gamma_d_roots)
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

n = 40;
[beta, S, mu] = polyfit((1:n)', price(N-n+1:N), 1);
beta_t = 2.0e-2;

if gamma_dv > gamma_dvt && beta(1) > -beta_t
    trend = 1;
elseif gamma_dv < -gamma_dvt && beta(1) < beta_t
    trend = -1;
else
    trend = 0;
end

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

% M = min([N, 2000]);
[alpha, S, mu] = polyfit((1:N)', price(1:N), 1);
if isempty(max_alpha) || alpha(1) > max_alpha
    max_alpha = alpha(1);
    pillar_idx = N;
end

M = min(1000, N);
[zeta, S, mu] = polyfit((1:M)', price(N-M+1:N), 4);
zeta_d = polyder(zeta);
zeta_dv = polyval(zeta_d, M, [], mu);

zeta_d2 = polyder(zeta_d);
zeta_d_roots = roots(zeta_d);
zeta_d_roots = zeta_d_roots(...
    zeta_d_roots == real(zeta_d_roots));

zeta_d_roots = round(zeta_d_roots*std(1:M) + mean(1:M));
zeta_d_roots = unique(zeta_d_roots);
zeta_d_roots = zeta_d_roots(zeta_d_roots >= 1 & ...
                            zeta_d_roots <= M);
switch length(zeta_d_roots)
  case 0
    if zeta_dv > 0
        category = 0;
    else
        category = 1;
    end
  case 1
    if polyval(zeta_d2, zeta_d_roots(1), [], mu) > 0
        category = 10;
    else
        category = 11;
    end
  case 2
    if (polyval(zeta_d2, zeta_d_roots(2), [], mu) > 0 &&...
        zeta_d_roots(2) - zeta_d_roots(1) > 100)
        category = 20;
    else
        category = 21;
    end
  case 3
    if (polyval(zeta_d2, zeta_d_roots(3), [], mu) > 0 &&...
        zeta_d_roots(3) - zeta_d_roots(2) > 100)
        category = 30;
    else
        category = 31;
    end
  otherwise % This is of course never the case
    category = -1;
end

abslevel = sum(price <= price(end))/N;
minp = min(price);
up = (price(end) - minp)/minp;
dist = std(price(end-M+1:end))/mean(price(end-M+1:end));
% flag = flag && dist > 2.6e-3;
% if alpha(1) > 0.1
%     uplim = 2.0e-2 - 6.67e-2 * (alpha(1) - 1.0e-1);
% end

if mystatus == 0
    flag = 0;
    switch category
      case {0, 10, 20, 30}
        flag = 1;
        flag = flag && alpha(1) > -0.1;
        flag = flag && trend == 1;
        flag = flag && up < 0.01;
        msg = sprintf(['[%s] price=%.2f; N=%d; alpha(1)=%e; dist=%e '...
                       'abslevel=%e; up=%e; category=%d'], ...
                      nowtime, price(end), N, alpha(1), ...
                      dist, abslevel, up, category);
        
      otherwise
        if price(N) < price(1) * (1 - 2.6e-2)
            flag = pivotal == -1;
            flag = flag && trend == 1;
            flag = flag && zeta_dv > 0;
            flag = flag && datenum(nowtime) > datenum('16:00');
            saving_meal = 1;
        end
        msg = sprintf(['[%s] price=%.2f; N=%d; price(1)=%f; '...
                       'category=%d'], nowtime, ...
                      price(end), N, price(1), category);

    end
    if (flag)
        action = xxl_act(mymode, mystatus);
    end
elseif mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity, price(end));
    flag = profit > 0;
    flag = flag && ~saving_meal;
    flag = flag && trend == -1;
    flag = flag && sum([1, 11, 21, 31] == category);
    if flag
        action = xxl_act(mymode, mystatus);
    end
    msg = sprintf(['[%s] price=%.2f; N=%d; ' ...
                   'profit=%f; abslevel=%e; '...
                   'gamma_dv=%e; zeta_dv=%e; '...
                   'category=%d; Escape'], ...
                  nowtime, price(end), N, profit, abslevel, ...
                  gamma_dv, zeta_dv, category);
end
