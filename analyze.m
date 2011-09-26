clear all
prices = importdata('/tmp/prices.dat');
X = importdata('/tmp/indicators.10min');
Y = importdata('/tmp/indicators.2hour');
Z = importdata('/tmp/indicators.day');
% colume number of the average, high and low
av = 1;
hi = 2;
lo = 3;

N = 178;

plot(1:N, prices(4:N+3), ...
     1:N, X(:, av), ...
     1:N, X(:, lo), ...
     1:N, X(:, hi), ...
     1:N, Z(4:N+3, av));

