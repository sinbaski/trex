% function [newspec, action, retcode, msg] = ...
%         analyze(my_position, price, volume, spec, flags, dbinfo, mysql)

% fprintf(2, '[%s]\n', dbinfo{1, 2});
% [newspec, action, retcode, msg] = ...
%     analyze2(my_position, price, volume, spec, flags, dbinfo, mysql);

function [action, retcode, msg] = analyze(my_position, dbinfo)

fprintf(2, '[%s]\n', dbinfo{1, 2});
[action, retcode, msg] = xxl_linsis(my_position, dbinfo);
