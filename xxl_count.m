% Count the number of elements in a matrix that satisfy a given
% condition
function counts = xxl_count(X, cond)
[rows columns] = size(X);
counts = zeros(rows, 1);
for a = 1:rows
    for b = 1:columns
        if cond(X(a, b))
            counts(a, 1) = counts(a, 1) + 1;
        end
    end
end
