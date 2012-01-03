function avgs = average_price(X, weights, n)
l = length(X);
if l < n
    avgs = [];
    return;
end

avgs = NaN(idivide(int32(l), int32(n)), 1);
r = mod(length(X), n);
for l = length(avgs):-1:1
    b = l * n + r;
    % a = b - n + 1;
    % avgs(l) = (X(a : b)' * weights(a : b)) / sum(weights(a : b));
    avgs(l) = X(b);
end
% for k = 0:num-1
%     if ~weighted
%         avgs(k + 1) = mean(X(n - size * (num - k) + 1 :...
%                              n - size * (num - k - 1), 1));
%     else
%         avgs(k + 1) = weighted_average(X(n - size * (num - k) + 1 :...
%                              n - size * (num - k - 1), :));
%     end
% end


