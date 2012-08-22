function y = timediff(t1, t2)
t1 = t1(4:6);
t2 = t2(4:6);
x = t2 - t1;
y = sum(x .* [3600, 60, 1]);
