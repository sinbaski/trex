#include "utilities.h"

const char *get_tick_size(double price)
{
	const char *ticksize;

	if (price <= 0.4999) ticksize = "0.0001";
	else if (price <= 0.9995) ticksize = "0.0005";
	else if (price <= 1.999) ticksize = "0.0010";
	else if (price <= 4.998) ticksize = "0.0020";
	else if (price <= 9.995) ticksize = "0.0050";
	else if (price <= 49.99) ticksize = "0.01";
	else if (price <= 99.95) ticksize = "0.05";
	else if (price <= 499.9) ticksize = "0.1";
	else if (price <= 999.5) ticksize = "0.5";
	else if (price <= 4999.0) ticksize = "1.0";
	else if (price <= 9995.0) ticksize = "5.0";
	else if (price <= 19990.0) ticksize = "10.0";
	else if (price <= 39980.0) ticksize = "20.0";
	else if (price <= 49960.0) ticksize = "40.0";
	else if (price <= 79950.0) ticksize = "50.0";
	else if (price <= 99920.0) ticksize = "80.0";
	else ticksize = "100.0";
	return ticksize;
}

/**
 * dignum: # of digits after the decimal point.
 * return: a.b -> a x 10^len(b) + b
 */
long str2long(const char *str, int *dignum)
{
	int l;
	long ret;
	const char *p;
	l = strlen(str);
	p = strstr(str, ".");
	if (p == NULL) *dignum = 0;
	else *dignum = l - (p - str + 1);

	/* No decimal part */
	if (*dignum == 0) {
		sscanf(p, "%lu", &ret);
	} else {
		long dec;
		sscanf(str, "%lu.%lu", &ret, &dec);
		ret = ret * intpow(10, *dignum) + dec;
	}
	return ret;
}

int pricecmp(const char *pstr1, const char *pstr2)
{
	int l1, l2;
	if ((l1 = strlen(pstr1)) != (l2 = strlen(pstr2)))
		return l1 - l2;
	else
		return strcmp(pstr1, pstr2);
}

GString *deincreament_price(GString *price, const char *ticksize, int up)
{
	long quanta, x;
	int a, b, c;

	quanta = str2long(ticksize, &b);
	x = str2long(price->str, &a);
	c = a > b ? a : b;
	if (a < b)
		x = x * intpow(10, b - a) +
			(up ? quanta : -quanta);
	else if (a == b)
		x += (up ? quanta : -quanta);
	else
		x += (up ? quanta : -quanta) *
			intpow(10, a - b);

	g_string_printf(price, "%ld", x);
	if (c) {
		g_string_insert(price, price->len - c, ".");
		if (*price->str == '.')
			g_string_prepend_c(price, '0');
	}
	return price;
}

GString *round_price(double price, const char *ticksize,
		     enum rounding_scheme scheme)
{
	GString *gstr = g_string_new(NULL);
	long quanta, x;
	long u, v;
	int dignum;

	quanta = str2long(ticksize, &dignum);
	g_string_printf(gstr, "%.*f", dignum, price);
	x = str2long(gstr->str, &dignum);
	u = x / quanta;
	v = x % quanta;
	if (scheme == round_up && v) u++;
	else if (scheme == round_nearest && v > (quanta >> 1)) u++;
	g_string_printf(gstr, "%ld", u * quanta);
	if (dignum) {
		g_string_insert(gstr, gstr->len - dignum, ".");
		if (*gstr->str == '.')
			g_string_prepend_c(gstr, '0');
	}
	return gstr;
}

GString *make_valid_price(double price)
{
	return round_price(price, get_tick_size(price),
			   round_nearest);
}

time_t parse_time(const char *timestring)
{
       struct tm *tm;
       time_t x;

       time(&x);
       tm = localtime(&x);
       sscanf(timestring, "%d:%d:%d", &tm->tm_hour, &tm->tm_min, &tm->tm_sec);
       x = mktime(tm);
       return x;
}

