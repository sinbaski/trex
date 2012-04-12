% function [newspec, action, retcode, msg] = ...
%         analyze(my_position, price, volume, spec, flags, dbinfo, mysql)

% fprintf(2, '[%s]\n', dbinfo{1, 2});
% [newspec, action, retcode, msg] = ...
%     analyze2(my_position, price, volume, spec, flags, dbinfo, mysql);

function [newspec, action, retcode, msg] = ...
        analyze(my_position, price, volume, spec, flags, dbinfo, mysql)

fprintf(2, '[%s]\n', dbinfo{1, 2});
[newspec, action, retcode, msg] = analyze2(my_position, dbinfo, mysql);
