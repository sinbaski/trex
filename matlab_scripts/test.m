clear all

mysql = database('avanza', 'sinbaski', 'q1w2e3r4',...
                 'com.mysql.jdbc.Driver', ...
                 'jdbc:mysql://localhost:3306/avanza');

myposition={0, 0, 0, 1300, '00:00'};
dbinfo = {'2012-01-20', '14:25:09', 'Nordea_Bank', mysql};
[action, ret, msg] = xxl_linsis9(myposition, dbinfo);

close(mysql);
