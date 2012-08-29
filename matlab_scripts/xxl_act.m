function action = xxl_act(mode, from_status)

SELL = int8(-1);
NONE = int8(0);
BUY = int8(1);
% if (mode == 0 && from_status == 0 ||
%     mode == 1 && from_status == 1)
%     action = BUY;
% elseif (mode == 0 && from_status == 1 ||
%     mode == 1 && from_status == 0)
%     action = SELL;
% end
if mode == from_status
    action = BUY;
else
    action = SELL;
end
