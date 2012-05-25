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

windowsize = 25;
windownum = 16;

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

P = average_price(price(1 : end), quantity(1 : end), windowsize);
RT = price2ret(P);

[newspec, errors1, LLF1, residuals1, sigmas1, summary1] = ...
    xxl_fit(spec, RT, prefitted);
fprintf(2, 'xxl_fit has returned. %s.\n', summary1.converge);
if isempty(strfind(summary1.converge, 'converged'))
    action = int8(NONE);
    retcode = int8(-1);
    msg = sprintf('The model does not converge.\n');
    return;
end

if ~simulate
    action = int8(NONE);
    retcode = int8(0);
    msg = sprintf('No simulation.\n');
    return;
end
[sim_inn1, sim_sig1, sim_rt1] = garchsim(newspec, windownum, numPaths, ...
                                         [], [], [], ...
                                         residuals1, sigmas1, RT);
sim_p1 = ret2price(sim_rt1, P(end));
% sim_p1 = sim_p1(2:end, :);
% Calculate the minimum price difference

clear prefitted errors1 LLF1 residuals1 sigmas1 summary1 P RT

avg = mean(sim_p1')';
[cmax, imax] = max(avg);
[cmin, imin] = min(avg);

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

r = double(2.5) / double(80);
if abs(price2ret([price(end); cmax])) > r || ...
        abs(price2ret([price(end); cmin])) > r
    retcode = int8(1);
    msg = sprintf('[%s] The simulation result is insane', char(T(end)));
    return;
end
clear r

if mymode == 0 && mystatus == 0
    if imin == 1 && cmax > avg(1) + mindiff    
        action = int8(BUY);
    end
    msg = sprintf('[%s] price=%f + %f; imin = %d; imax = %d; cmax = %f',...
                  char(T(end)), price(end), ...
                  mindiff, imin, imax, cmax);
elseif mymode == 0 && mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity,...
                        price(end), feerate);
    if imax == 1 && (profit > 0 || price2ret([myprice; cmin]) < -0.5/80)
        action = int8(SELL);
    end
    msg = sprintf('[%s] price=%f; profit = %f; imax = %d',...
                  char(T(end)), price(end), profit, imax);
elseif mymode == 1 && mystatus == 0
    if imax == 1 && cmin < avg(1) - mindiff    
        action = int8(SELL);
    end
     msg = sprintf('[%s] price=%f - %f; imin = %d; imax = %d; cmin = %f',...
                   char(T(end)), price(end), mindiff, imin, imax, cmin);
elseif mymode == 1 && mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity,...
                        price(end), feerate);
    if imin == 1 && (profit > 0 || price2ret([myprice; cmax]) > 0.5/80)
        action = int8(BUY);
    end
    msg = sprintf('[%s] price=%f; profit = %f; imin = %d',...
                  char(T(end)), price(end), profit, imin);
end
