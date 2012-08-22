clear all

mysql = database('avanza', 'sinbaski', 'q1w2e3r4',...
                 'com.mysql.jdbc.Driver', ...
                 'jdbc:mysql://localhost:3306/avanza');

myposition={0, 0, 0, 1300, '00:00'};
dbinfo = {'2012-08-06', '13:50:02', 'Lundin_Mining_Corp', mysql};
[action, ret, msg] = xxl_chaser(myposition, dbinfo);

close(mysql);
