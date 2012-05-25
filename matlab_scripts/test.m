clear all

mysql = database('avanza', 'sinbaski', 'q1w2e3r4',...
                 'com.mysql.jdbc.Driver', ...
                 'jdbc:mysql://localhost:3306/avanza');


% X = fetch(mysql, ['select price, volume from Nordea_Bank where tid like ' ...
%                   '"2012-04-26 %" order by tid asc limit 1035;']);
% X = fetch(mysql, ['select price, volume from Ericsson_B where tid like ' ...
%                   '"2012-05-11 %" order by tid asc;']);

X = fetch(mysql, ['select price, volume from Tele2_B where tid like ' ...
                 '"2012-05-15 %" order by tid;']);
% X = flipud(X);

close(mysql);
price = cell2mat(X(:, 1));
volume = cell2mat(X(:, 2));

N = length(price);
n = 400;

coeff=polyfit((1:N)', price, 1);
Y = polyval(coeff, (1:N)');
error = std(Y - price)/mean(price);

coeff2 = polyfit((1:n)', price(end-n+1:end), 1);
Y2 = polyval(coeff2, (1:n)');

%subplot(2, 1, 1);
plot(1:N, price, 1:N, Y, N-n+1:N, Y2);
grid on

% subplot(2, 1, 2);
% n = floor(log2(N/40));
% periods = (0:n)';
% periods = flipud(periods);
% periods = 2 .^ periods;
% periods = periods .* 40;

% [logRS,logERS,V]=RSana(price(end - n*40 + 1 : end), periods*40, 'Hurst', 1);
% plot(log10(periods), logERS);
% coeff3 = polyfit(log10(periods), logERS, 1);
% fprintf('H = %e\n', coeff3(1));
% grid on



