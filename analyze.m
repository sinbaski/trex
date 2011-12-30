function action = analyze(stock)
% 1000 trades in total. approx. 1 hour
numPaths = 1000;

N1 = 20;
M1 = 50;

N2 = 80;
spec = [], spec2 = [];

action_buy = 0;
action_sell = 1;
action_none = 2;
persistent call_count = 0;
persistent spec1 = garchset('Distribution' , 'T'  , 'Display', 'off', ...
                'VarianceModel', 'GJR', 'P', 1, 'Q', 1, ...
                'R', 1, 'M', 1);
persistent spec2 = garchset('Distribution' , 'T'  , 'Display', 'off', ...
                'VarianceModel', 'GJR', 'P', 1, 'Q', 1, ...
                'R', 1, 'M', 1);

[where, T, price, quantity] = textread(strcat('records/', stock, '.dat'), ...
                                       '%s\t%s\t%f%d');
l = length(T) - N1 * M1 + 1;
P1 = average_price(price(l : end), quantity(l : end), N1);
l = mod(length(T), N2) + 1;
P2 = average_price(price(l : end), quantity(l : end), N2);

RT1 = price2ret(P1);
RT2 = price2ret(P2);

[spec1, errors1, LLF1, residuals1, sigmas1, summary1] = ...
xxl_fit(spec1, RT1, call_count == 0);
if isempty(strfind(summary1.converge, 'converged'))
    return action_none;
end

[spec2, errors2, LLF2, residuals2, sigmas2, summary2] = ...
xxl_fit(spec2, RT2, call_count == 0);
if isempty(strfind(summary2.converge, 'converged'))
    return action_none;
end

call_count = call_count + 1;
[sim_inn1, sim_sig1, sim_rt1] = garchsim(spec1, N1 * M1, numPaths, ...
                                         [], [], [], ...
                                         residuals1, sigmas1, RT1);
[sim_inn2, sim_sig2, sim_rt2] = garchsim(spec2, N1 * M1, numPaths, ...
                                         [], [], [], ...
                                         residuals2, sigmas2, RT2);
p1_sim = ret2price(rt1_sim, P1(end));
p2_sim = ret2price(rt2_sim, P2(end));

cond = @(arg) 