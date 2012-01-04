% returns = price2ret(price);
% autocorr(returns.^2);
% title('Sample ACF of Squared Returns');

if ~xxl_prefitted
    specx = garchset('Distribution' , 'T'  , 'Display', 'off', ...
                     'VarianceModel', 'GJR', 'P', 1, 'Q', 1, ...
                     'R', 1, 'M', 1);
end

specx = analyze('183828', specx, xxl_prefitted, '2011-11-14', 0);
