% Count the number of elements in a matrix that satisfy a given
% condition
function num = xxl_count(X, cond)
[rows columns] = size(X);
num = 0;
for a = 1:rows
    for b = 1:columns
        if cond(X(a, b))
            num = num + 1;
        end
    end
end
