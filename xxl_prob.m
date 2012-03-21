function probs = xxl_prob(X, cond)
[rows columns] = size(X);
probs = double(xxl_count(X, cond)) / double(columns);
