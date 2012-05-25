function [newspec, action, retcode, msg] = ...
        analyze(my_position, price, volume, spec, flags, now, border)

BUY = 0;
SELL = 1;
NONE = 2;

% N = windownum * windowsize;

policy = struct('min_open_profit', 140, 'min_close_profit', 140);

mymode = my_position{1, 1};
mystatus = my_position{1, 2};
myprice = my_position{1, 3};
myquantity = my_position{1, 4};
feerate = 0.08e-2;

%statistical check
% if length(price(border:end)) < 500 && mystatus == 0
%     newspec = spec;
%     action = int8(NONE);
%     retcode = 0;
% elseif (length(price(border:end)) >= 500 && std(price(border:end))/ ...
%         mean(price(border:end)) < 2e-3 && )
% end

numPaths = 2000;
windowsize = 40;
windownum = 80;
castnum = 15;

P = average_price(price, volume, windowsize);
RT = price2ret(P);

mindiff  = policy.min_open_profit + ...
    2 * price(end) * feerate * myquantity;
mindiff = double(mindiff) / double(myquantity);
if mymode == 0
    mindiff = mindiff / (1 - feerate);
else
    mindiff = mindiff / (1 + feerate);
end

% we first do a linear fit
%index = (1:windownum)';
N = 500;
index = (1:N)';
coeff = polyfit(index, price(end - N + 1:end), 1);
Y = polyval(coeff, index);
error = mean(abs(Y - price(end - N + 1:end)) ./ Y);
if error <= 1.0e-3
%if error <= 1.5e-3
    newspec = spec;
    retcode = int8(0);
    action = int8(NONE);
    if mystatus == 0
        if mymode == 0
            price_level = sum(price(end-N+1:end) > price(end) + ...
                              mindiff)/N;
        else
            price_level = sum(price(end-N+1:end) < price(end) - ...
                              mindiff)/N;
        end
        % if abs(coeff(1)) < 1.0e-2 && price_level > 0.7
        if abs(coeff(1)) < 6.0e-4 && price_level > 0.7
            action = mymode;
        % elseif (coeff(1) > 1.0e-2 && mymode == 0 && price_level ...
        %         > 0.5)
        elseif (coeff(1) > 6.0e-4 && mymode == 0 && price_level ...
                > 0.55)
            action = int8(BUY);
        % elseif (coeff(1) < -1.0e-2 && mymode == 1 && price_level ...
        %         > 0.5)
        elseif (coeff(1) < -6.0e-4 && mymode == 1 && price_level ...
                > 0.55)
            action = int8(SELL);
        end
        msg = sprintf('[%s] price=%f + %f; Y=%e X + %f; error=%e; price_level=%f', ...
                      now, price(end), mindiff, coeff(1), coeff(2), error, price_level);
    else
        % a = 895.61;
        % b = -11.48;
        profit = xxl_profit(mymode, myprice, myquantity,...
                            price(end), feerate);
        if mymode == 0
            price_level = sum(price(end-N+1:end) < ...
                              price(end))/N;
        else
            price_level = sum(price(end-N+1:end) > ...
                              price(end))/N;
        end

        if (mymode == 0 && profit > 0 && price_level > 0.9)
            action = int8(SELL);
        % elseif (mymode == 0 && coeff(1) > 1.0e-2 &&...
        %         price_level < 0.95)
        %     action = int8(NONE);
            
        elseif (mymode == 1 && profit > 0 && price_level > 0.9)
            action = int8(BUY);
        % elseif (mymode == 1 && coeff(1) < -1.0e-2 &&...
        %         price_level < 0.95)
        %     action = int8(NONE);
            
        % elseif (mymode == 0 && coeff(1) < -1.0e-2 &&...
        %         profit > 0) 
        elseif (mymode == 0 && coeff(1) < -6.0e-4 &&...
                profit > 0) 
            action = int8(SELL);            
        % elseif (mymode == 1 && coeff(1) > 1.0e-2 &&...
        %         profit > 0)
        elseif (mymode == 1 && coeff(1) > 6.0e-4 &&...
                profit > 0)
            action = int8(BUY);            
        end
        msg = sprintf(...
            ['[%s] price=%f; Y=%e X + %f; error=%e; ' ...
             'profit=%f; price_level = %f'],...
            now, price(end), coeff(1), coeff(2), error, ...
            profit, price_level);
    end
    return;
end
clear Y index N

[newspec, errors, LLF, residuals, sigmas, summary] = ...
    xxl_fit(spec, RT, bitget(flags, [1]));
fprintf(2, 'xxl_fit has returned. %s.\n', summary.converge);
if isempty(strfind(summary.converge, 'converged'))
    if mystatus == 0
        action = int8(NONE);
        msg = sprintf('[%s] divergent', now);
    else
        profit = xxl_profit(mymode, myprice, myquantity,...
                            price(end), feerate);
        if profit < 0
            action = int8(NONE);
        elseif mymode == 0
            action = int8(SELL);
        else
            action = int8(BUY);
        end
        msg = sprintf('[%s] price=%f; profit=%f; divergent', now, ...
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
                  now, price(end), mindiff, max(prob), price_level);
elseif mymode == 0 && mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity,...
                        price(end), feerate);
    for k = 1:castnum
        prob(k) = sum(forecast(k, :) > price(end))/numPaths;
    end
    price_level = sum(price(end-N+1:end) < price(end))/N;
    if (profit > 0 && ((max(prob) < 0.85 && price_level > 0.9) || ...
                       max(prob) < 0.5))
        action = int8(SELL);
    end
    msg = sprintf('[%s] price=%f; myprice=%f; profit=%f; prob=%f; price_level=%f',...
                  now, price(end), myprice, profit, max(prob), price_level);
elseif mymode == 1 && mystatus == 0
    for k = 1:castnum
        prob(k) = sum(forecast(k, :) < price(end) - mindiff)/numPaths;
    end
    price_level = sum(price(end-N+1:end) < price(end) - mindiff)/N;
    if max(prob) > 0.8 && price_level > 0.55
        action = int8(SELL);
    end
    msg = sprintf('[%s] price=%f + %f; prob=%f; price_level=%f',...
                  now, price(end), mindiff, max(prob), price_level);
elseif mymode == 1 && mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity,...
                        price(end), feerate);
    for k = 1:castnum
        prob(k) = sum(forecast(k, :) < price(end))/numPaths;
    end
    price_level = sum(price(end-N+1:end) > price(end))/N;
    if (profit > 0 && ((max(prob) < 0.85 && price_level > 0.9) || ...
                       max(prob) < 0.5))
        action = int8(BUY);
    end
    msg = sprintf('[%s] price=%f; myprice=%f; profit=%f; prob=%f; price_level=%f',...
                  now, price(end), myprice, profit, max(prob), price_level);
end
