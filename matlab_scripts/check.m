clear all

mysql = database('avanza', 'sinbaski', 'q1w2e3r4',...
                 'com.mysql.jdbc.Driver', ...
                 'jdbc:mysql://localhost:3306/avanza');

timefmt = 'HH:MM:SS';
% name = 'Ericsson_B';
% day = '2012-03-27';
% x = 5643;

% name = 'TeliaSonera';
% day = '2012-07-05';
% x = 1734;

% name = 'ABB_Ltd';
% day = '2012-08-08';
% x = 1030;

name = 'Volvo_B';
day = '2012-08-09';
x = 3210;

% name = 'Sandvik';
% day = '2012-07-20';
% x = 2465;

stmt = sprintf(['select time(tid), price from %s where tid like ' ...
                '"%s %%" order by tid asc;'], name, day);
data = fetch(mysql, stmt);
close(mysql);
N = size(data, 1);
tid = datevec(data(:, 1), timefmt);
price = cell2mat(data(:, 2));
t2 = datevec(data(x, 1), timefmt);
t1 = [0, 0, 0, 9, 0, 0];
indice = ones(1, 3);
for a = x-1:-1:1
    if indice(1) == 1 && timediff(tid(a, :), t2) >= 20*60
        indice(1) = a;
    elseif indice(2) == 1 && timediff(tid(a, :), t2) >= 75*60
        indice(2) = a;
    % elseif indice(3) == 1 && timediff(tid(a, :), t2) >= 7200
    %     indice(3) = a;
    %     break;
    end
end


% X = fetch(mysql, ['select price, volume from TeliaSonera where tid like ' ...
%                   '"2012-06-12 %" order by tid asc;']);
% X = fetch(mysql, ['select price, volume from Boliden where tid like ' ...
%                   '"2012-01-09 %" order by tid asc;']);

% X = fetch(mysql, ['select price, volume from Nordea_Bank where tid like ' ...
%                   '"2012-01-18 %" order by tid;']);
%X = flipud(X);




% Trend in the last 20 minutes.
n = x-indice(1)+1;
[gamma, S, mu] = polyfit((x-n+1:x)', price(x-n+1:x), 3);
gamma_v = polyval(gamma, (x-n+1:x)', S, mu);
gamma_d = polyder(gamma);
gamma_dv = polyval(gamma_d, x, [], mu);

% trend in the last hour
M = x-indice(2)+1;
% pzeta = tsmovavg(price(x-M+1:x), 's', round(M/200), 1);
[zeta, S, mu] = polyfit((x-M+1:x)', price(x-M+1:x), 4);
zeta_v = polyval(zeta, (x-M+1:x)', S, mu);
zeta_d = polyder(zeta);
zeta_dv = polyval(zeta_d, x, [], mu);

minp = min(price(1:x));
maxp = max(price(1:x));
abslevel = (price(x) - minp)/(maxp - minp);

dist = std(price(x-M+1:end))/mean(price(x-M+1:end));

fprintf('%s %s x=%d:\n', name, day, x);
fprintf(['gamma_dv=%e\n'...
         'zeta_dv=%e\n'...
         'abslevel=%e\n'...
         'dist=%e\n'],...
        gamma_dv, zeta_dv, abslevel, dist);

%subplot(2, 1, 1);
plot(1:N, price, '.',...
     x-length(gamma_v)+1:x, gamma_v,...
     x-length(zeta_v)+1:x, zeta_v);


% GARCH model from here onward.

% avg = time_average(price(1:x), tid(1:x, :), 60);

% subplot(2, 1, 2);
% plot(1:length(avg), avg);
% grid on

% spec = garchset('Distribution' , 'T',...
%                 'Display', 'off', ...
%                 'VarianceModel', 'GARCH',...
%                 'P', 1, 'Q', 1, ...
%                 'R', 5, 'M', 5);

% rets = price2ret(avg);

% [spec, errors, LLF, residuals, sigmas, summary] = ...
%     garchfit(spec, rets);

% fprintf('%s. Exitflag=%d\n', summary.converge, summary.exitFlag);

% if summary.exitFlag > 0
%     [sigmaF, meanF] = garchpred(spec, rets, 10);
    
%     priceF = ret2price(meanF, avg(end), [], 0, 'periodic');
%     meanF
% end

