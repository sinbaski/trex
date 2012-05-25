clear all

mysql = database('avanza', 'sinbaski', 'q1w2e3r4',...
                 'com.mysql.jdbc.Driver', ...
                 'jdbc:mysql://localhost:3306/avanza');


% X = fetch(mysql, ['select price, volume from Nordea_Bank where tid like ' ...
%                   '"2012-04-26 %" order by tid asc limit 1035;']);
X = fetch(mysql, ['select price, volume from Nordea_Bank where tid like ' ...
                  '"2012-04-26 %" order by tid asc limit 6000;']);
close(mysql);
price = cell2mat(X(:, 1));
m = 1:150;
n = m*40;

[logRS,logERS,V]=RSana(price, n, 'Hurst', 1);
plot(log10(m), logRS, log10(m), logERS);
