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

windowsize = 40;
windownum = 80;
castnum = 15;
N = windownum * windowsize;

policy = struct('min_open_profit', 140, 'min_close_profit', 140);

fprintf(2, 'analyze has started.\n');

calfile = fopen(strcat('records/', stock, '-', ...
                       calibration, '.dat'), 'r');
datfile = fopen(strcat('records/', stock, '-', ...
                       datestr(today, 'yyyy-mm-dd'), ...
                       '.dat'), 'r');
feerate = 0.08E-2;

n1 = fnum_of_line(calfile);
n2 = fnum_of_line(datfile);

if n1 + n2 < N || n2 < 800
% if n1 + n2 < N
    retcode = int8(-3);
    action = int8(NONE);
    msg = sprintf('Not enough data. n1 = %d, n2 = %d', n1, n2);
    newspec = spec;
    return;
end

T = cell(n1 + n2, 1);
price = NaN(n1 + n2, 1);
quantity = NaN(n1 + n2, 1);

C = textscan(calfile, '%*s\t%s\t%f\t%d', n1);
T(1 : n1) = C{1};
price(1:n1, 1) = C{2};
quantity(1:n1, 1) = C{3};

C = textscan(datfile, '%*s\t%s\t%f\t%d', n2);
T(n1 + 1 : end) = C{1};
price(n1 + 1 : end, 1) = C{2};
quantity(n1 + 1 : end, 1) = C{3};

fclose(calfile);
fclose(datfile);

% We are done with loading data
clear C n1 n2 calfile datfile fd

P = average_price(price(end - N + 1 : end), ...
                  quantity(end - N + 1 : end), ...
                  windowsize);
RT = price2ret(P);

index = 1:windownum;
% we first do a linear fit
coeff = polyfit(index', P, 1);
Y = transpose(polyval(coeff, index));
error = mean(abs(Y - P) ./ Y);
if error <= 3.0E-3
    newspec = spec;
    retcode = int8(0);
    maxerr = max(abs(Y - P) ./ Y);
    nowerr = (price(end) - Y(end))/Y(end);
    if mystatus == 0 && abs(nowerr)/maxerr >= 0.7
        if coeff(1) > 0 && nowerr < 0 && mymode == 0 
            action = int8(BUY);
        elseif coeff(1) < 0 &&  nowerr > 0 && mymode == 1
            action = int8(SELL);
        end
        msg = sprintf('[%s] price=%f; maxerr = %f; nowerr = %f; error = %f',...
                      char(T(end)), price(end), maxerr, nowerr, error);
    elseif mystatus == 1
        profit = xxl_profit(mymode, myprice, myquantity,...
                            price(end), feerate);
        if profit > 0
            if mymode == 0 && nowerr > maxerr * 0.65
                action = int8(SELL);
            elseif mymode == 1 && nowerr < -maxerr * 0.65
                action = int8(BUY);
            else
                action = int8(NONE);
            end
        else
            if mymode == 0 && myprice > Y(end) * (1 + maxerr) && ...
                       coeff(1) < 0
                action = int8(SELL);
            elseif mymode == 1 && myprice < Y(end) * (1 - maxerr) && ...
                       coeff(1) > 0
                action = int8(BUY);
            else
                action = int8(NONE);
            end
        end

        msg = sprintf('[%s] price=%f; maxerr = %f; nowerr = %f; error = %f; profit = %f', ...
                       char(T(end)), price(end), maxerr, nowerr, error, profit);
    else
        action = int8(NONE);
        msg = sprintf('[%s] price=%f; maxerr = %f; nowerr = %f; error = %f', ...
                      char(T(end)), price(end), maxerr, nowerr, error);
    end
    
    return;
end
clear Y index error maxerr nowerr

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
    msg = sprintf('[%s] No simulation.', char(T(end)));
    return;
end
[siminn, simsig, simret] = ...
    garchsim(newspec, castnum, numPaths, ...
             [], [], [], ...
             residuals, sigmas, RT);
forecast = ret2price(simret, price(end));
% forecast = forecast(2:end, :);
% Calculate the minimum price difference

%rt = mean(price2ret(price(end - 40 + 1 : end))) * (castnum / 2);
clear prefitted errors LLF residuals sigmas summary P RT


mindiff  = policy.min_open_profit + ...
    2 * price(end) * feerate * myquantity;
mindiff = double(mindiff) / double(myquantity);
fprintf(2, 'mindiff = %f\n', mindiff);
if mymode == 0
    mindiff = mindiff / (1 - feerate);
else
    mindiff = mindiff / (1 + feerate);
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
    
    if max(prob) > 0.68
        action = int8(BUY);
    end
    msg = sprintf('[%s] price=%f + %f; prob = %f',...
                  char(T(end)), price(end), mindiff, max(prob));
elseif mymode == 0 && mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity,...
                        price(end), feerate);
    if profit > 0
        cond = @(p) p > price(end) + mindiff;
        prob = xxl_prob(forecast, cond);
        if max(prob) < 0.68
            action = int8(SELL);
        end
    elseif profit < 0
        cond = @(p) p > price(end);
        prob = xxl_prob(forecast, cond);
        if max(prob) < 0.68
            action = int8(SELL);
        end
    end
    msg = sprintf('[%s] price=%f; profit = %f; prob = %f',...
                  char(T(end)), price(end), profit, max(prob));
elseif mymode == 1 && mystatus == 0
    cond = @(p) p < price(end) - mindiff;
    prob = xxl_prob(forecast, cond);

    if max(prob) > 0.68
        action = int8(SELL);
    end
    msg = sprintf('[%s] price=%f + %f; prob = %f',...
                  char(T(end)), price(end), mindiff, max(prob));
elseif mymode == 1 && mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity,...
                        price(end), feerate);
    if profit > 0
        cond = @(p) p < price(end) - mindiff;
        prob = xxl_prob(forecast, cond);
        if max(prob) < 0.68
            action = int8(BUY);
        end
    elseif profit < 0
        cond = @(p) p < price(end);
        prob = xxl_prob(forecast, cond);
        if max(prob) < 0.68
            action = int8(BUY);
        end
    end
    msg = sprintf('[%s] price=%f; profit = %f; prob = %f',...
                  char(T(end)), price(end), profit, max(prob));
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
