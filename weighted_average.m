function avg = weighted_average(X)
total = 0;
avg = 0;
for k = 1:length(X)
    total = total + X(k, 2);
    avg = avg + X(k, 1) * X(k, 2) / total - avg * X(k, 2) / total;
end
