% Compute the hurst exponent
function h = hurst(X)
n = length(X);
m = mean(X);
Y = X - m;
Z = NaN(n, 1);

Z(1) = Y(1);
for t = 2:n
    Z(t) = Z(t - 1) + Y(t);
end
R = max(Z) - min(Z);
S = std(X, 1);
