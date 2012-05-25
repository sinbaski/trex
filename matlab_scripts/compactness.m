function mu = compactness(P, n)
mu = zeros(length(P) - n + 1, 1);
for k = n:length(P)
    ret = price2ret(P(k-n+1:k));
    mu(k-n+1) = sum(abs(ret)) / length(ret);
end

