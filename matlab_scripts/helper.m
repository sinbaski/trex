clear all

mysql = database('avanza', 'sinbaski', 'q1w2e3r4',...
                 'com.mysql.jdbc.Driver', ...
                 'jdbc:mysql://localhost:3306/avanza');

grpsize = 40;
X = fetch(mysql, ['select price, volume from Nordea_Bank where ' ...
                  'tid <= "2012-03-27 11:46:48" order by tid desc ' ...
                  'limit 3200']);
X = flipud(X);
price = cell2mat(X(:, 1));
volume = cell2mat(X(:, 2));

P = average_price(price, volume, grpsize);
N = length(P);

index = (1:N)';

coeff = polyfit(index, P, 1);
Y = transpose(polyval(coeff, 1:N));
error = mean(abs(Y - P) ./ Y);

fprintf('coeff = [%f %f]; error = %f.\n', coeff(1), coeff(2), error);

subplot(2, 1, 1);
plot(1:N, P, 1:N, Y);
grid on
subplot(2, 1, 2);
plot(1:N, abs(Y - P) ./ Y);
grid on

close(mysql);