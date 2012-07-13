clear all

mysql = database('avanza', 'sinbaski', 'q1w2e3r4',...
                 'com.mysql.jdbc.Driver', ...
                 'jdbc:mysql://localhost:3306/avanza');

% name = 'Ericsson_B';
% day = '2012-03-27';
% x = 5643;

% name = 'TeliaSonera';
% day = '2012-07-05';
% x = 1734;

name = 'Nordea_Bank';
day = '2012-07-05';
x = 2668;

stmt = sprintf(['select price, volume from %s where tid like ' ...
                '"%s %%" order by tid asc;'], name, day);
X = fetch(mysql, stmt);

% X = fetch(mysql, ['select price, volume from TeliaSonera where tid like ' ...
%                   '"2012-06-12 %" order by tid asc;']);
% X = fetch(mysql, ['select price, volume from Boliden where tid like ' ...
%                   '"2012-01-09 %" order by tid asc;']);

% X = fetch(mysql, ['select price, volume from Nordea_Bank where tid like ' ...
%                   '"2012-01-18 %" order by tid;']);
%X = flipud(X);

close(mysql);
price = cell2mat(X(:, 1));
volume = cell2mat(X(:, 2));
clear X
N = length(price);

%ret = price2ret(price(1:x));
% smoothness = sum(abs(ret))/(x-1);
%H = compute_hurst(ret);
%M = min([2000, x]);
%avg = average_price(price, volume, 10);

M = min(2000, x);
[alpha, S, mu] = polyfit((x-M+1:x)', price(x-M+1:x), 1);
alpha_v = polyval(alpha, (x-M+1:x)', S, mu);

errors = abs(alpha_v - price(x-M+1:x));
plevel = sign(price(x) - alpha_v(end))*...
         sum(errors < abs(alpha_v(end) - price(x)))/x;

n = min([400, x]);
[gamma, S, mu] = polyfit((x-n+1:x)', price(x-n+1:x), 3);
gamma_v = polyval(gamma, (x-n+1:x)', S, mu);
gamma_d = polyder(gamma);
gamma_dv = polyval(gamma_d, x, [], mu);

%ret60 = sum(price2ret(price(x-60+1:x)));

M = min([240, x]);
[beta, S, mu] = polyfit((x-M+1:x)', price(x-M+1:x), 1);
beta_v = polyval(beta, (x-M+1:x)', [], mu);

[zeta, S, mu] = polyfit((1:x)', price(1:x), 4);
zeta_v = polyval(zeta, (1:x)', S, mu);
zeta_d = polyder(zeta);
zeta_dv = polyval(zeta_d, x, [], mu);

%volatility = var(price2ret(price(1:x)));
%errors = mean(abs(beta_v - price(N-M+1:N)))/mean(price(N-M+1:N));
%Yder = polyval(Pder, (x-n+1:x)', S, mu);

%error = std(Y - price)/mean(price);

% P2 = polyfit((1:n)', price(end-n+1:end), 1);
% Y2 = polyval(P2, (1:n)');

%subplot(2, 1, 1);

fprintf('%s %s x=%d:\n', name, day, x);
% fprintf(['alpha(1)=%e, beta(1)=%e, gamma_dv=%e\nzeta_dv=%e, '...
%         'alpha_level=%e, abslevel=%e\n'], alpha(1), beta(1),...
%         gamma_dv, zeta_dv, plevel, sum(price(max(x-1000+1, 1):x) < ...
%                                        price(x))/min(x, 1000));
fprintf(['alpha(1)=%e, beta(1)=%e, gamma_dv=%e\nzeta_dv=%e, '...
        'alpha_level=%e, abslevel=%e\n'], alpha(1), beta(1),...
        gamma_dv, zeta_dv, plevel, sum(price(1:x) < ...
                                       price(x))/x);

%plot(1:N, price, x-n+1:x, Y3, x-M+1:x, beta_v);
%figure
plot(1:N, price, '.', x-length(alpha_v)+1:x, alpha_v, x-240+1:x, beta_v, x-n+1:x, gamma_v, ...
     1:x, zeta_v);

grid on

% price_ma = tsmovavg(price', 'e', 40);
% subplot(2, 1, 2);
% plot(1:length(price_ma), price_ma, x-n+1:x, gamma_v);
% grid on


% n = floor(log2(N/40));
% periods = (0:n)';
% periods = flipud(periods);
% periods = 2 .^ periods;
% periods = periods .* 40;

% [logRS,logERS,V]=RSana(price(end - n*40 + 1 : end), periods*40, 'Hurst', 1);
% plot(log10(periods), logERS);
% P3 = polyfit(log10(periods), logERS, 1);
% fprintf('H = %e\n', P3(1));
% grid on



