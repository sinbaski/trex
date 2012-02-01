% Test the GARCH model.
clear all
X = importdata('/tmp/boliden.dat');
len = length(X);

grpsize = 15;
grpnum = floor(len / grpsize);
R = 7;
M = 7;
P = 1;
Q = 1;
n = 20;

N = 27;

if mod(len, grpsize) ~= 0
    grpnum = grpnum + 1;
end

avgs = zeros(grpnum, 1);
for l = 1:grpnum
    avgs(l) = mean(X((l - 1) * grpsize + 1 : min(l * grpsize, len)));
end
rets = price2ret(avgs(1:N));
% autocorr(rets, 40, 20, 2);

coeff = garchset('VarianceModel', 'GARCH', 'R', R, 'M', M, 'P', P, 'Q', Q);
[coeff, errors, LLF, innovations, sigmas, summary] =...
    garchfit(coeff, rets);
[sig, m] = garchpred(coeff, rets, n);

forecast = ret2price([m - sig, m, m + sig], avgs(N));
plot(1:grpnum, avgs, 'x-', N:N + n, forecast);

% figure
% plot(1:length(rets), rets);
