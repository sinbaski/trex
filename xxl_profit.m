function profit = xxl_profit(mymode, myprice, myquantity, price, feerate)
if mymode == 0
    diff = price - myprice;
else
    diff = myprice - price;
end
profit = (diff - (price + myprice) * feerate) * myquantity;
