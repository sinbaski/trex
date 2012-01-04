function newspec = analyze(stock, spec, prefitted, calibration, ...
                           simulate)
% 1000 trades in total. approx. 1 hour
numPaths = 1000;

windowsize = 10;
windownum = 100;

BUY = 0;
SELL = 1;
NONE = 2;

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

fd = fopen(strcat('positions/', stock, '-', ...
                  datestr(today, 'yyyy-mm-dd'), '.pos'), 'r');
C = textscan(fd, '%d %d %f %d');
mymode = C{1};
mystatus = C{2};
myprice = C{3};
myquantity = C{4};
fclose(fd);

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
    fprintf(2, 'The model does not converge.\n');
    fprintf(fd, '%d %d\n', 0, 0);
    fclose(fd);
    return;
end

if ~simulate
    return
end
[sim_inn1, sim_sig1, sim_rt1] = garchsim(newspec, windownum, numPaths, ...
                                         [], [], [], ...
                                         residuals1, sigmas1, RT);
sim_p1 = ret2price(sim_rt1, P(end));
sim_p1 = sim_p1(2:end, :);
% Calculate the minimum price difference

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

% rt = mean(RT(end - 30 + 1 : end));
action = NONE;
fd = fopen(strcat('rendezvous/', stock, '.txt'), 'w');
if mymode == 0 && mystatus == 0
    cond = @(x) x > price(end) + mindiff;
    prob = xxl_prob(sim_p1, cond);
    if prob > 0.7
        action = BUY;
    else
        action = NONE;
    end
    fprintf(fd, '%d %d\n', 1, action);
    fprintf(fd, '[%s] price=%f + %f; prob = %f',...
            char(T(end)), price(end), mindiff, prob);
elseif mymode == 0 && mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity,...
                        price(end), feerate);
    if profit > 0
        threshold = 0.55;
    else
        threshold = 0.45;
    end
    cond = @(x) x > price(end);
    prob = xxl_prob(sim_p1, cond);
    if prob > threshold
        action = NONE;
    else
        action = SELL;
    end
    fprintf(fd, '%d %d\n', 1, action);
    fprintf(fd, '[%s] price=%f; profit = %f; prob = %f',...
            char(T(end)), price(end), profit, prob);
elseif mymode == 1 && mystatus == 0
    cond = @(x) x < price(end) - mindiff;
    prob = xxl_prob(sim_p1, cond);
    if prob > 0.7
        action = SELL;
    else
        action = NONE;
    end
    fprintf(fd, '%d %d\n', 1, action);
    fprintf(fd, '[%s] price=%f - %f; prob = %f',...
            char(T(end)), price(end), mindiff, prob);
elseif mymode == 1 && mystatus == 1
    profit = xxl_profit(mymode, myprice, myquantity, price(end), feerate);
    if profit > 0
        threshold = 0.55;
    else
        threshold = 0.45;
    end
    cond = @(x) x < price(end);
    prob = xxl_prob(sim_p1, cond);
    if prob > threshold
        action = NONE;
    else
        action = BUY;
    end
    fprintf(fd, '%d %d\n', 1, action);
    fprintf(fd, '[%s] price=%f; profit = %f; prob = %f',...
            char(T(end)), price(end), profit, prob);
end
fclose(fd);
