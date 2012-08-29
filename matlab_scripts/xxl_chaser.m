function [action, retcode, msg] = xxl_chaser(my_position, dbinfo)

SELL = int8(-1);
NONE = int8(0);
BUY = int8(1);
timefmt = 'HH:MM:SS';

mymode = my_position{1, 1};
mystatus = my_position{1, 2};
myprice = my_position{1, 3};
myquantity = my_position{1, 4};
myquota = my_position{1, 5};
mytime = my_position{1, 6};

today = dbinfo{1, 1};
nowtime = dbinfo{1, 2};
tbl_name = dbinfo{1, 3};
mysql = dbinfo{1, 4};

%persistent max_alpha;
%persistent pillar_idx;
t2 = datevec(nowtime, timefmt);
t1 = [0, 0, 0, 9, 0, 0];
indice = ones(1, 3);
persistent top_price;
persistent exit_rate;
persistent viscous

if isempty(top_price)
    stmt = sprintf(['select top_price, exit_rate, viscous '...
                    'from parameters where co_name="%s";'],...
                   tbl_name);
    data = fetch(mysql, stmt);
    top_price = cell2mat(data(1, 1));
    exit_rate = cell2mat(data(1, 2));
    viscous = cell2mat(data(1, 3));
end

stmt = sprintf(['select time(tid), price from %s where tid like "%s ' ...
                '%%" and time(tid) <= "%s" order by tid asc;'], ...
               tbl_name, today, nowtime);
data = fetch(mysql, stmt);
N = size(data, 1);
tid = datevec(data(:, 1), timefmt);
price = cell2mat(data(:, 2));
for a = N-1:-1:1
    if indice(1) == 1 && timediff(tid(a, :), t2) >= 20*60
        indice(1) = a;
    elseif indice(2) == 1 && timediff(tid(a, :), t2) >= 75*60
        indice(2) = a;
    % elseif indice(3) == 1 && timediff(tid(a, :), t2) >= 7200
    %     indice(3) = a;
    %     break;
    end
end

retcode = int8(0);
action = NONE;
msg = sprintf('[%s]\n', nowtime);

if mymode == 1
    msg = sprintf('[%s] Mode 1 is not supported.\n', nowtime);
    return;
end

% if (N < 800 && mystatus == 0 ||...
%     N < 100 && mystatus == 1)
%     msg = sprintf('[%s] Only %d trades.\n',...
%                   nowtime, length(price));
%     return;
% end

if (timediff([0, 0, 0, 10, 0, 0], t2) < 0 && mystatus == 0 ||...
    timediff([0, 0, 0, 9, 5, 0], t2) < 0 && mystatus == 1)
    msg = sprintf('[%s] Only %d trades.\n', nowtime, N);
    return;
end

% n = max([N-indice(1)+1, 400]);
% n = min([n, N]);
n = N-indice(1)+1;
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

% n = 40;
% [beta, S, mu] = polyfit((1:n)', price(N-n+1:N), 1);
% beta_t = 2.0e-2;

if gamma_dv > gamma_dvt % && beta(1) > -beta_t
    trend = 1;
elseif gamma_dv < -gamma_dvt % && beta(1) < beta_t
    trend = -1;
else
    trend = 0;
end

%good_rate = 3.0e-3;
if mystatus == 1 && timediff([0, 0, 0, 10, 0, 0], t2) < 0
    profit = xxl_profit(mymode, myprice, myquantity, price(end));
    if (profit > myprice*myquantity*exit_rate &&...
        (~viscous && trend == -1 || viscous))

        action = xxl_act(mymode, mystatus);
        msg = sprintf(...
            ['[%s] price=%f; N=%d; profit=%f; gamma=%f'], nowtime, ...
            price(end), N, profit, gamma_dv);
        return;
    end
end

% M = max([N-indice(2)+1, 1000]);
% M = min([M, N]);
M = N-indice(2)+1;
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
    if (polyval(zeta_d2, zeta_d_roots(2), [], mu) > 0)
        category = 20;
    else
        category = 21;
    end
  case 3
    if (polyval(zeta_d2, zeta_d_roots(3), [], mu) > 0)
        category = 30;
    else
        category = 31;
    end
  otherwise % This is of course never the case
    category = -1;
end

minp = min(price);
maxp = max(price);
mindiff = xxl_mindiff(my_position, price(end), exit_rate);
abslevel = (price(end) - minp)/(maxp - minp);
%up = (price(end) - minp)/minp;
dist = std(price(end-M+1:end))/mean(price(end-M+1:end));
if mystatus == 0
    flag = 0;
    switch category
      case {0, 10, 20, 30}
        % [alpha, S, mu] = polyfit((1:N)', price(1:N), 1);
        entry = 0;
        
        flag = 1;
        flag = flag && price(end) < top_price;
        flag = flag && (maxp - minp) > mindiff * 2;
        flag = flag && pivotal == -1;
        flag = flag && trend == 1;
        flag = flag && abslevel < 0.5;
        flag = flag && dist > 1.0e-3;

        msg = sprintf(['[%s] price=%.2f; N=%d; gamma_dv=%e; dist=%e; '...
                       'abslevel=%e; category=%d; price(1)=%.2f; '...
                       'entry=%d'], ...
                      nowtime, price(end), N, gamma_dv, ...
                      dist, abslevel, category, price(1), entry);
      otherwise
        msg = sprintf(['[%s] price=%.2f; N=%d; price(1)=%f; '...
                       'category=%d'], nowtime, ...
                      price(end), N, price(1), category);
    end
    if (flag)
        action = xxl_act(mymode, mystatus);
    end
elseif mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity, price(end));
    flag = profit > myprice * myquantity * exit_rate;
    if ~viscous
        flag = flag && trend == -1;
        flag = flag && sum([1, 11, 21, 31] == category);
        flag = flag && dist > 1.0e-3;
    end
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
