function prob = xxl_prob(X, cond)
prob = double(xxl_count(X, cond)) / prod(size(X));
