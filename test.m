clear all

mysql = database('avanza', 'sinbaski', 'q1w2e3r4',...
                 'com.mysql.jdbc.Driver', ...
                 'jdbc:mysql://localhost:3306/avanza');


X = fetch(mysql, ['select price, volume from Nordea_Bank where tid like ' ...
                  '"2012-04-12 %" order by tid asc;']);
% X = fetch(mysql, ['select price, volume from Nordea_Bank where tid like ' ...
%                   '"2012-02-24 %" and tid <= "2012-02-24 11:44:32" ' ...
%                   'order by tid desc;']);
%X = flipud(X);
price = cell2mat(X(:, 1));
N = length(price);
coeff=polyfit((1:N)', price, 1);
Y = polyval(coeff, (1:N)');
error = std(Y - price)/mean(price);
%error = mean(abs(Y - price)./Y);
[coeff, std(Y - price), error]
%subplot(2, 1, 1);
plot(1:N, price, 1:N, polyval(coeff, 1:N));
grid on
% subplot(2, 1, 2);
% plot(1:length(volume), volume);
% grid on
close(mysql);

% X = fetch(mysql, ['select price from Nordea_Bank where tid like ' ...
%                     '"2012-02-23 %" and tid <= "2012-02-23 15:54:56" ' ...
%                     'order by tid desc limit 500;']);
% X = flipud(X);
% price = cell2mat(X(:, 1));
% coeff=polyfit((1:500)', price, 1);
% Y = polyval(coeff, (1:500)');
% error = mean(abs(Y - price)./Y);
% [coeff, mean(abs(Y - price)), error]
% %volume = cell2mat(X(:, 2));
% subplot(2, 1, 2);
% plot(1:500, price, 1:500, polyval(coeff, 1:500));
% grid on
% % subplot(3, 1, 3);
% % plot(1:length(volume), volume);
% % grid on
% %close(mysql);
