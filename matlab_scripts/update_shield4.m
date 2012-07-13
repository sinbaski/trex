function shield = update_shield4(mymode, beta1, mysql, co_name, status)

persistent surge;
persistent plunge;
persistent p;
persistent q;
persistent n;
persistent w;
persistent w_old;
persistent initial_rush;
persistent up_expan;
persistent down_expan;

shield = status;

if isempty(n)
    n = 1;
    p = beta1;
    q = beta1;
    w = 0;
    w_old = 0;
    initial_rush = 1;
    stmt = sprintf(['select up_expan, down_expan from ' ...
                    'parameters where co_name = "%s";'], co_name);
    data = fetch(mysql, stmt);
    up_expan = cell2mat(data(:, 1));
    down_expan = cell2mat(data(:, 2));
end

if n > 1
    if beta1 > q
        w = 1;
    elseif beta1 >= p && beta1 <= q
        w = 0;
    else
        w = -1;
    end
end
if n == 2
    w_old = w;
end
if w_old ~= w
    initial_rush = 0;
end

fprintf(2, 'min(betadist) = %e, max(betadist)=%e\n', p, q);

if shield == 0
    surge = 0;
    plunge = 0;
end

threshold = 3.0e-2;
switch shield
  case 0
    shield = shield_condition(mymode, beta1, p, q, up_expan, down_expan, ...
                              initial_rush, shield);
  
  case {1, 11}
    a = mymode == 0 && beta1 < -threshold;
    a = a || mymode == 1 && beta1 > threshold;
    if (a)
        shield = shield  + 1;
    end
    
  % case 11
  %   a = mymode == 0 && beta1 < -threshold;
  %   a = a || mymode == 1 && beta1 > threshold;
  %   if (a)
  %       shield = -2;
  %   end
    
  case 2
    a = mymode == 0 && beta1 > threshold;
    a = a || mymode == 1 && beta1 < -threshold;
    if (a)
        surge = surge + 1;
    elseif surge ~= 0
        surge = 0;
    end
    if (surge >= 1)
        shield = 0;
    end
    
  case 12
    a = mymode == 0 && beta1 > threshold;
    a = a || mymode == 1 && beta1 < -threshold;
    if (a)
        plunge = plunge + 1;
    elseif surge ~= 0
        plunge = 0;
    end
    if plunge >= 1
        shield = 0;
    end
end
n = n + 1;
w_old = w;
p = min([p, beta1]);
q = max([q, beta1]);
