function avgs = averages(X, n, weighted)
num = 4;
size = 15;
avgs = zeros(1, num);

for k = 0:num-1
    if ~weighted
        avgs(k + 1) = mean(X(n - size * (num - k) + 1 :...
                             n - size * (num - k - 1), 1));
    else
        avgs(k + 1) = weighted_average(X(n - size * (num - k) + 1 :...
                             n - size * (num - k - 1), :));
    end
end


