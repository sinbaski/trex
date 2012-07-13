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

% persistent shield;
% persistent enable_shield;
persistent histrory;
persistent cursor;
persistent max_alpha;
persistent marker;
% persistent counter1;
% persistent counter2;

if isempty(marker)
    marker = 0;
end

if isempty(histrory)
    histrory = zeros(1, 1000);
    cursor = 2;
    % counter1 = 0;
    % counter2 = 0;
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

if (N < 200 && mystatus == 0 ||...
    N < 100 && mystatus == 1)
    msg = sprintf('[%s] Only %d trades.\n',...
                  nowtime, length(price));
    return;
end

M = min(2000, N);
[alpha, S, mu] = polyfit((1:M)', price(end-M+1:end), 1);
alpha_v = polyval(alpha, (1:M)', [], mu);
errors = abs(alpha_v - price(end-M+1:end));

if isempty(max_alpha)
    max_alpha = alpha(1);
else
    max_alpha = max(alpha(1), max_alpha);
end

% M = min([N, 240]);
% if M == N
%     beta = alpha;
% else
%     [beta, S, mu] = polyfit((1:M)', price(end-M+1:end), 1);
% end
M = min([N, 200]);
[beta, S, mu] = polyfit((1:M)', price(end-M+1:end), 1);

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
histrory(cursor) = beta(1);
% shield = histrory(cursor);
% if shield ~= histrory(cursor - 1) && counter2 ~= 0
%     counter2 = 0;
% end
cursor = cursor + 1;

if mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity, price(end));
    plevel = sign(price(end) - alpha_v(end))*...
             sum(errors < abs(alpha_v(end) - price(end)))/ ...
             length(alpha_v);
    abslevel = sum(price < price(end))/N;
    msg = sprintf(...
        ['[%s] price=%f; N=%d; profit=%f; plevel=%f; gamma_dv=%e'],...
        nowtime, price(end), N, profit, plevel, gamma_dv);
    
    flag = profit > myprice*myquantity*good_rate;
    %flag = profit > 0;
    % flag = trend == -1;    
    flag = flag && (histrory(cursor-2) > 0 &&...
                    histrory(cursor-1) < 0);
    if flag
        action = xxl_act(mymode, mystatus);
        return;
    end

    % flag = trend == -1;
    % flag = flag && profit > 0;
    % flag = flag && (profit > myprice*myquantity*good_rate ||...
    %                 plevel > 0.825 || abslevel > 0.9);
    % if flag
    %     action = xxl_act(mymode, mystatus);
    %     return;
    % end
    
else
    flag = mymode == 0;
    if max_alpha > 1.0e-1
        flag = flag && (alpha(1) > max_alpha/2 || alpha(1) < -max_alpha)
    end
    flag = flag && histrory(cursor-2) < 0 && histrory(cursor-1) > 0;
    % flag = flag && shield == 1;
    % flag = flag && histrory(cursor - 1) ~= 1;
    %flag = flag && trend == 1;
    if flag
        action = xxl_act(mymode, mystatus);
    end
    msg = sprintf(['[%s] price=%f; N=%d; alpha(1)=%e; '...
                   'beta(1)=%e; gamma_dv=%e'],...
                  nowtime, price(end), N, alpha(1), ...
                  beta(1), gamma_dv);
    return;
    % flag = mymode == 0;
    % flag = flag && (shield == 2 || shield == 12);
    % flag = flag && counter2 == 0;
    % flag = flag && beta(1) > 0;
    % flag = flag && trend == 1;
    % if flag
    %     action = xxl_act(mymode, mystatus);
    %     counter2 = counter2 + 1;
    %     msg = sprintf(['[%s] price=%f; N=%d; beta(1)=%f; gamma_dv=%e; shield=%d'],...
    %                   nowtime, price(end), N, beta(1), gamma_dv, shield);
    %     return;
    % end
    
    % flag = mymode == 0;
    % flag = flag && (enable_shield || n <= 7 || n >= 20);
    % flag = flag && shield;
    % if flag
    %     msg = sprintf(['[%s] price=%f; N=%d; beta(1)=%f; shield=%d'],...
    %                   nowtime, price(end), N, beta(1), shield);
    %     return;
    % end
end

% threshold = 0.825;
% clear n;

% if (mystatus == 0 && N < 1000)
%         msg = sprintf('[%s] Only %d trades.\n',...
%                   nowtime, length(price));
%         return;
% end

% plevel = sign(price(end) - alpha_v(end))*...
%     sum(errors < abs(alpha_v(end) - price(end)))/N;
% if mystatus == 0
%     mindiff = xxl_mindiff(my_position, price(end));
%     flag = 1;
%     if (mymode == 0)
%         % flag = flag && (price(end) < alpha_v(end) - mindiff*0.75 && ...
%         %                 plevel < -threshold);

%         flag = flag && plevel < -threshold;
%         flag = flag && alpha(1) > -1.0e-1;
%         flag = flag && trend == 1;
%         flag = flag && ~(alpha(1) > 2.0e-1 && beta(1) < -3.20e-2);
%     else
%         % flag = flag && (price(end) > alpha_v(end) + mindiff*0.75 && ...
%         %                 plevel > threshold);

%         flag = flag && plevel > threshold;
%         flag = flag && alpha(1) < 1.0e-1;
%         flag = flag && trend == -1;
%         flag = flag && ~(alpha(1) < -2.0e-1 && beta(1) > 3.20e-2);
%     end
%     if (flag)
%         action = xxl_act(mymode, mystatus);
%     end
%     msg = sprintf(['[%s] price=%.2f + %.2f; Y=%.2f; N=%d; ' ...
%                    'plevel=%f; lim=[%f %f]; trend=[%e %e %e]'], ...
%                   nowtime, ...
%                   price(end), mindiff, alpha_v(end), N, ...
%                   plevel, alpha_v(end) - mindiff*0.75, ...
%                   alpha_v(end) + mindiff*0.75, alpha(1), beta(1), ...
%                   gamma_dv);
% elseif mystatus == 1
%     profit = xxl_profit(mymode, myprice, myquantity, price(end));
%     abslevel = sum(price < price(end))/N;
%     flag = profit > 0;
%     if (mymode == 0)
%         flag = flag && (plevel > threshold || abslevel > 0.9);
%         flag = flag && trend == -1;
%         flag = flag && (mymode == 0 && shield ~= 1 ||...
%                         mymode == 1 && shield ~= 11)
%     else
%         flag = flag && (plevel < -threshold || abslevel < 0.1);
%         flag = flag && trend == 1;
%     end
%     if flag
%         action = xxl_act(mymode, mystatus);
%     end
%     msg = sprintf(...
%             ['[%s] price=%.2f; Y=%.2f; N=%d; ' ...
%              'profit=%f; plevel=%f; abslevel=%e; '...
%              'beta(1)=%e; gamma_dv=%e; shield=%d'], ...
%         nowtime, price(end), alpha_v(end), N, profit, ...
%         plevel, abslevel, beta(1), gamma_dv, shield);
% end
