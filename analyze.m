function [newspec, action, retcode, msg] = ...
        analyze(stock, mymode, mystatus, myprice, myquantity,...
                spec, prefitted, calibration, simulate)
BUY = 0;
SELL = 1;
NONE = 2;

if strcmp(calibration, 'no')
    retcode = int8(-2);
    action = int8(NONE);
    msg = sprintf('No calibration data.');
    newspec = spec;
    return;
end

numPaths = 2000;

windowsize = 20;
windownum = 25;
castnum = 10;
N = windownum * windowsize;

policy = struct('min_open_profit', 140, 'min_close_profit', 140);

fprintf(2, 'analyze has started.\n');

% calfile = fopen(strcat('records/', stock, '-', ...
%                        calibration, '.dat'), 'r');
datfile = fopen(strcat('records/', stock, '-', ...
                       datestr(today, 'yyyy-mm-dd'), ...
                       '.dat'), 'r');
feerate = 0.08E-2;

% n1 = fnum_of_line(calfile);
n1 = 0;
n2 = fnum_of_line(datfile);

if n2 < N
    retcode = int8(-3);
    action = int8(NONE);
    msg = sprintf('Not enough data.');
    newspec = spec;
    return;
end

T = cell(n1 + n2, 1);
price = NaN(n1 + n2, 1);
quantity = NaN(n1 + n2, 1);

% C = textscan(calfile, '%*s\t%s\t%f\t%d', n1);
% T(1 : n1) = C{1};
% price(1:n1, 1) = C{2};
% quantity(1:n1, 1) = C{3};

C = textscan(datfile, '%*s\t%s\t%f\t%d', n2);
T(n1 + 1 : end) = C{1};
price(n1 + 1 : end, 1) = C{2};
quantity(n1 + 1 : end, 1) = C{3};

% fclose(calfile);
fclose(datfile);

% We are done with loading data
clear C n1 n2 calfile datfile fd

P = average_price(price(end - N + 1 : end), ...
                  quantity(end - N + 1 : end), ...
                  windowsize);
RT = price2ret(P);

[newspec, errors, LLF, residuals, sigmas, summary] = ...
    xxl_fit(spec, RT, prefitted);
fprintf(2, 'xxl_fit has returned. %s.\n', summary.converge);
if isempty(strfind(summary.converge, 'converged'))
    if mystatus == 0
        action = int8(NONE);
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
    end
    retcode = int8(-1);
    msg = sprintf('[%s] The model does not converge', char(T(end)));
    return;
end

if ~simulate
    action = int8(NONE);
    retcode = int8(0);
    msg = sprintf('[%s] No simulation.\n', char(T(end)));
    return;
end
[siminn, simsig, simret] = ...
    garchsim(newspec, castnum, numPaths, ...
             [], [], [], ...
             residuals, sigmas, RT);
forecast = ret2price(simret, price(end));
% forecast = forecast(2:end, :);
% Calculate the minimum price difference

clear prefitted errors LLF residuals sigmas summary P RT

if mystatus == 0
    mindiff  = policy.min_open_profit + ...
        2 * price(end) * feerate * myquantity;
    mindiff = double(mindiff) / double(myquantity);
    fprintf(2, 'mindiff = %f\n', mindiff);
    if mymode == 0
        mindiff = mindiff / (1 - feerate);
    else
        mindiff = mindiff / (1 + feerate);
    end
end

retcode = int8(0);
action = int8(NONE);

% r = double(3.0) / double(80);
% if abs(price2ret([price(end); max(max(forecast))])) > r || ...
%         abs(price2ret([price(end); min(min(forecast))])) > r
%     retcode = int8(1);
%     msg = sprintf('[%s] The simulation result is insane', char(T(end)));
%     return;
% end
% clear r

if mymode == 0 && mystatus == 0
    cond = @(p) p > price(end) + mindiff;
    prob = xxl_prob(forecast, cond);
    
    cond = @(p) p > price(end);
    prob0 = xxl_prob(price(end - N + 1 : end), cond);
    if prob0 > 0.7 && prob > 0.6
        action = int8(BUY);
    end
    msg = sprintf('[%s] price=%f + %f; prob0 = %f; prob = %f',...
                  char(T(end)), price(end), mindiff, prob0, prob);
elseif mymode == 0 && mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity,...
                        price(end), feerate);
    cond = @(p) p > price(end);
    prob = xxl_prob(forecast, cond);

    if profit > 0 && prob < 0.5
        action = int8(SELL);
    elseif profit < 0
            cond = @(p) price2ret([myprice; p]) < -0.5E-2;
            prob = xxl_prob(forecast, cond);
            if prob > 0.5
                action = int8(SELL);
            end
    end
    msg = sprintf('[%s] price=%f; profit = %f; prob = %f',...
                  char(T(end)), price(end), profit, prob);
elseif mymode == 1 && mystatus == 0
    cond = @(p) p < price(end) - mindiff;
    prob = xxl_prob(forecast, cond);

    cond = @(p) p < price(end);
    prob0 = xxl_prob(price(end - N + 1 : end), cond);

    if prob0 > 0.7 && prob > 0.6
        action = int8(SELL);
    end
    msg = sprintf('[%s] price=%f + %f; prob0 = %f; prob = %f',...
                  char(T(end)), price(end), mindiff, prob0, prob);
elseif mymode == 1 && mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity,...
                        price(end), feerate);
    cond = @(p) p < price(end);
    prob = xxl_prob(forecast, cond);

    if profit > 0 && prob < 0.5
        action = int8(BUY);
    elseif profit < 0
            cond = @(p) price2ret([myprice; p]) > 0.5E-2;
            prob = xxl_prob(forecast, cond);
            if prob > 0.5
                action = int8(BUY);
            end
    end
    msg = sprintf('[%s] price=%f; profit = %f; prob = %f',...
                  char(T(end)), price(end), profit, prob);

end

% elseif mymode == 1 && mystatus == 0
%     if imax == 1 && cmin < avg(1) - mindiff
%         action = int8(SELL);
%     end
%      msg = sprintf('[%s] price=%f - %f; imin = %d; imax = %d; cmin = %f',...
%                    char(T(end)), price(end), mindiff, imin, imax, cmin);
% elseif mymode == 1 && mystatus == 1
%     profit = xxl_profit(mymode, myprice, myquantity,...
%                         price(end), feerate);
%     if imin == 1 && (profit > 0 || price2ret([myprice; cmax]) > 0.5/80)
%         action = int8(BUY);
%     end
%     msg = sprintf('[%s] price=%f; profit = %f; imin = %d',...
%                   char(T(end)), price(end), profit, imin);
% end
