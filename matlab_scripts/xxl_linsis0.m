function [action, retcode, msg, error] = xxl_linsis(my_position, ...
                                                  price, volume, ...
                                                  allowed_error)
BUY = 0;
SELL = 1;
NONE = 2;

mymode = my_position{1, 1};
mystatus = my_position{1, 2};
myprice = my_position{1, 3};
myquantity = my_position{1, 4};

mindiff = xxl_mindiff(my_position, price(end));

retcode = int8(0);
action = int8(NONE);
msg='';
    
N = length(price);
index = (1:N)';
coeff = polyfit(index, price, 1);
Y = polyval(coeff, index);
errors = abs(Y - price);
error = mean(errors ./ Y);
if error >= allowed_error
    return;
end

N = min([N, 500]);
if mymode == 0
    price_level = sum(price > price(end))/N;
else
    price_level = sum(price < price(end))/N;
end
clear N

if mystatus == 0
    % if abs(coeff(1)) < 1.0e-2 && price_level > 0.7
    if abs(coeff(1)) < 6.0e-4 && price_level > 0.7
        action = mymode;
        % elseif (coeff(1) > 1.0e-2 && mymode == 0 && price_level ...
        %         > 0.5)
    elseif (coeff(1) > 6.0e-4 && mymode == 0 && price_level ...
            > 0.55)
        action = int8(BUY);
        % elseif (coeff(1) < -1.0e-2 && mymode == 1 && price_level ...
        %         > 0.5)
    elseif (coeff(1) < -6.0e-4 && mymode == 1 && price_level ...
            > 0.55)
        action = int8(SELL);
    end
    msg = sprintf('[%s] price=%f + %f; Y=%e X + %f; error=%e; price_level=%f', ...
                  now, price(end), mindiff, coeff(1), coeff(2), error, price_level);
else
    % a = 895.61;
    % b = -11.48;
    profit = xxl_profit(mymode, myprice, myquantity, price(end));
    if (mymode == 0 && profit > 0 && price_level < 0.1)
        action = int8(SELL);
        % elseif (mymode == 0 && coeff(1) > 1.0e-2 &&...
        %         price_level < 0.95)
        %     action = int8(NONE);
        
    elseif (mymode == 1 && profit > 0 && price_level < 0.1)
        action = int8(BUY);
        % elseif (mymode == 1 && coeff(1) < -1.0e-2 &&...
        %         price_level < 0.95)
        %     action = int8(NONE);
        
        % elseif (mymode == 0 && coeff(1) < -1.0e-2 &&...
        %         profit > 0) 
    elseif (mymode == 0 && coeff(1) < -6.0e-4 &&...
            profit > 0) 
        action = int8(SELL);            
        % elseif (mymode == 1 && coeff(1) > 1.0e-2 &&...
        %         profit > 0)
    elseif (mymode == 1 && coeff(1) > 6.0e-4 &&...
            profit > 0)
        action = int8(BUY);            
    end
    msg = sprintf(...
        ['[%s] price=%f; Y=%e X + %f; error=%e; ' ...
         'profit=%f; price_level=%f'],...
        now, price(end), coeff(1), coeff(2), error, ...
        profit, price_level);
end
return;
