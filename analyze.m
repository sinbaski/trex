function newspec = analyze(stock, spec, prefitted)
% 1000 trades in total. approx. 1 hour
numPaths = 1000;

windowsize = 20;
windownum = 50;

BUY = 0;
SELL = 1;
NONE = 2;

fprintf(2, 'analyze has started.\n');
fd = fopen(strcat('rendezvous/', stock, '.txt'), 'w');
feerate = 0.08E-2;

policy = struct('min_open_profit', 140, 'min_close_profit', 140);

[where, T, price, quantity] = textread(strcat('records/', stock, '-', ...
                                              datestr(today, 'yyyy-mm-dd'), ...
                                              '.dat'), '%s\t%s\t%f\t%d');

[mymode, mystatus, myprice, myquantity] =...
    textread(strcat('positions/', stock, '-', datestr(today, 'yyyy-mm-dd'), ...
                    '.pos'), '%d %d %f %d');

P1 = average_price(price(1 : end), quantity(1 : end), windowsize);
RT1 = price2ret(P1);

[newspec, errors1, LLF1, residuals1, sigmas1, summary1] = ...
    xxl_fit(spec, RT1, prefitted);
fprintf(2, 'xxl_fit has returned. %s.\n', summary1.converge);
if isempty(strfind(summary1.converge, 'converged'))
    fprintf(2, 'The model does not converge.\n');
    fprintf(fd, '%d %d\n', 0, 0);
    fclose(fd);
    return;
end

[sim_inn1, sim_sig1, sim_rt1] = garchsim(newspec, windownum, numPaths, ...
                                         [], [], [], ...
                                         residuals1, sigmas1, RT1);
sim_p1 = ret2price(sim_rt1, P1(end));
sim_p1 = sim_p1(2:end, :);
% Calculate the minimum price difference
if mystatus == 0
    mindiff  = policy.min_open_profit + ...
        2 * price(end) * feerate * myquantity;
    mindiff = mindiff / myquantity;
    if mymode == 0
        mindiff = mindiff / (1 - feerate);
    else
        mindiff = mindiff / (1 + feerate);
    end
end

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
    profit = xxl_profit(mymode, myprice, myquantity, price(end), feerate);
    if profit > 0
        threshold = 0.7;
    else
        threshold = 0.6;
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
        threshold = 0.7;
    else
        threshold = 0.6;
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
