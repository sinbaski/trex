% shows the relationship between the price and volume.
clear all
grp_size = 40;

M = importdata('/tmp/data.dat');
price = M(:, 1);
volume = M(:, 2);

clear M
grp_num = idivide(int16(length(price)), int16(grp_size), 'floor');

c = mod(length(price), grp_size);
X = zeros(grp_num, 2);
Y = zeros(grp_num, 1);

P = zeros(grp_num, 2);
Q = zeros(grp_num, 1);
for a = 1:grp_num
    m = c + (a - 1)*grp_size + 1;
    n = c + a*grp_size;
    X(a, 1) = mean(price(m : n));
    Y(a) = sum(volume(m : n));
    X(a, 2) = sum(price(m : n) .* volume(m : n)) / Y(a);
end

% z = mean(X(:, 1));
% P(:, 1) = X(:, 1)./z;

% z = mean(X(:, 2));
% P(:, 2) = X(:, 2)./z;

% z = mean(Y(:, 1));
% Q(:, 1) = Y(:, 1)./z;

subplot(2, 1, 1);
plot(1:grp_num, X(:, 1), 1:grp_num, X(:, 2));
grid on
subplot(2, 1, 2);
plot(1:grp_num, Y(:, 1));

%legend('straight average', 'Volume-weighted average', 'volume');
grid on
