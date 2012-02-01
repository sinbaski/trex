% returns = price2ret(price);
% autocorr(returns.^2);
% title('Sample ACF of Squared Returns');

if ~xxl_prefitted
    specx = garchset('Distribution' , 'T'  , 'Display', 'off', ...
                     'VarianceModel', 'GJR', 'P', 1, 'Q', 1, ...
                     'R', 1, 'M', 1);
end

[specx, action, retcode, msg] = ...
    analyze('183828', 0, 0, double(0), 730, specx, xxl_prefitted, '2012-01-09', 1);
