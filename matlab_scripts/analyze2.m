function [newspec, action, retcode, msg] = ...
        analyze2(my_position, price, volume, spec,...
                 flags, dbinfo, mysql)
    
BUY = 0;
SELL = 1;
NONE = 2;

% N = windownum * windowsize;

policy = struct('min_open_profit', 140, 'min_close_profit', 140);

mymode = my_position{1, 1};
mystatus = my_position{1, 2};
myprice = my_position{1, 3};
myquantity = my_position{1, 4};

mindiff = xxl_mindiff(my_position, price(end));

today = dbinfo{1, 1};
nowtime = dbinfo{1, 2};
tbl_name = dbinfo{1, 3};

stmt = sprintf(['select count(*) from %s where tid like "%s %%" and ' ...
                'time(tid) <= "%s"'], tbl_name, today, nowtime);
N = fetch(mysql, stmt);
N = N{1, 1};
E1 = 1.4e-3 * (N/500);
fprintf(2, ['%d records in price while %d lines for ' ...
            'today!'], length(price), N);
if length(price) < N
    return;
end
if N > 250
    newspec = spec;
    [action, retcode, msg1, error] =...
        xxl_linsis(my_position, price(end - N + 1:end),...
                   volume(end - N + 1:end), E1);
    msg = sprintf('[%s] %s', nowtime, msg1);
    if error <= E1
        fprintf(2, 'linear analysis fits.\n');
        return;
    end
end

numPaths = 2000;
windowsize = 40;
windownum = 80;
castnum = 15;

P = average_price(price, volume, windowsize);
RT = price2ret(P);

[newspec, errors, LLF, residuals, sigmas, summary] = ...
    xxl_fit(spec, RT, bitget(flags, [1]));
fprintf(2, 'xxl_fit has returned. %s.\n', summary.converge);
if isempty(strfind(summary.converge, 'converged'))
    if mystatus == 0
        action = int8(NONE);
        msg = sprintf('[%s] divergent', nowtime);
    else
        profit = xxl_profit(mymode, myprice, myquantity,...
                            price(end));
        if profit < 0
            action = int8(NONE);
        elseif mymode == 0
            action = int8(SELL);
        else
            action = int8(BUY);
        end
        msg = sprintf('[%s] price=%f; profit=%f; divergent', nowtime, ...
                      price(end), profit);
    end
    retcode = int8(-1);
    return;
end

[siminn, simsig, simret] = ...
    garchsim(newspec, castnum, numPaths, ...
             [], [], [], ...
             residuals, sigmas, RT);
forecast = ret2price(simret, price(end));
clear prefitted errors LLF residuals sigmas summary P RT

retcode = int8(0);
action = int8(NONE);

N = 500;
prob = zeros(castnum, 1);
if mymode == 0 && mystatus == 0
    for k = 1:castnum
        prob(k) = sum(forecast(k, :) > price(end) + mindiff)/numPaths;
    end
    price_level = sum(price(end-N+1:end) > price(end) + mindiff)/N;
    if max(prob) > 0.8 && price_level > 0.55
        action = int8(BUY);
    end
    msg = sprintf('[%s] price=%f + %f; prob=%f; price_level=%f',...
                  nowtime, price(end), mindiff, max(prob), price_level);
elseif mymode == 0 && mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity,...
                        price(end));
    for k = 1:castnum
        prob(k) = sum(forecast(k, :) > price(end))/numPaths;
    end
    price_level = sum(price(end-N+1:end) < price(end))/N;
    if (profit > 0 && ((max(prob) < 0.85 && price_level > 0.9) || ...
                       max(prob) < 0.5))
        action = int8(SELL);
    end
    msg = sprintf('[%s] price=%f; myprice=%f; profit=%f; prob=%f; price_level=%f',...
                  nowtime, price(end), myprice, profit, max(prob), price_level);
elseif mymode == 1 && mystatus == 0
    for k = 1:castnum
        prob(k) = sum(forecast(k, :) < price(end) - mindiff)/numPaths;
    end
    price_level = sum(price(end-N+1:end) < price(end) - mindiff)/N;
    if max(prob) > 0.8 && price_level > 0.55
        action = int8(SELL);
    end
    msg = sprintf('[%s] price=%f + %f; prob=%f; price_level=%f',...
                  nowtime, price(end), mindiff, max(prob), price_level);
elseif mymode == 1 && mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity,...
                        price(end));
    for k = 1:castnum
        prob(k) = sum(forecast(k, :) < price(end))/numPaths;
    end
    price_level = sum(price(end-N+1:end) > price(end))/N;
    if (profit > 0 && ((max(prob) < 0.85 && price_level > 0.9) || ...
                       max(prob) < 0.5))
        action = int8(BUY);
    end
    msg = sprintf('[%s] price=%f; myprice=%f; profit=%f; prob=%f; price_level=%f',...
                  nowtime, price(end), myprice, profit, max(prob), price_level);
end
