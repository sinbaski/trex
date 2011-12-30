#ifndef INTRADAY_UTILITIES_H
#define INTRADAY_UTILITIES_H
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>

#define intpow(base, expo)					\
	({							\
		typeof(expo) i;					\
		typeof(base) x;					\
		for (i = 0, x = 1; i < (expo); i++)		\
			x *= (base);				\
		x;						\
	})

enum rounding_scheme {
	round_down,
	round_nearest,
	round_up,
};
long str2long(const char *str, int *dignum);
int pricecmp(const char *pstr1, const char *pstr2);
GString *round_price(double price, const char *ticksize,
		     enum rounding_scheme scheme);
GString *make_valid_price(double price);
#endif
