#ifndef _TRADE_H_
#define _TRADE_H_
#include <time.h>

#define SEC_AN_HOUR 3600
#define DATA_ROW_WIDTH 32

#define get_filename(dir, ext)						\
	({								\
		char filename[50];					\
		memset(filename, 0, sizeof(filename));			\
		strncpy(filename, dir, strlen(dir));			\
		if (!g_str_has_suffix(dir, "/"))			\
			filename[strlen(dir)] = '/';			\
		sprintf(filename + strlen(filename), "%s-%s",		\
			stockinfo.dataid,					\
			todays_date);					\
		if (!g_str_has_prefix(ext, "."))			\
			filename[strlen(filename)] = '.';		\
		strncpy(filename + strlen(filename), ext, strlen(ext)); \
		filename;						\
	})

#define get_timestring()					\
	({							\
		char str[9];					\
		time_t now;					\
		struct tm *timep;				\
		memset(str, 0, sizeof(str));			\
		time(&now);					\
		timep = localtime(&now);			\
		sprintf(str, "%02d:%02d:%02d", timep->tm_hour,	\
			timep->tm_min, timep->tm_sec);		\
		str;						\
	})

#define get_datestring()						\
	({								\
		char str[9];						\
		time_t now;						\
		struct tm *timep;					\
		memset(str, 0, sizeof(str));				\
		time(&now);						\
		timep = localtime(&now);				\
		sprintf(str, "%4d-%02d-%02d", timep->tm_year + 1900,	\
			timep->tm_mon + 1, timep->tm_mday);		\
		str;							\
	})

#define make_timestring(x)					\
	({							\
		char ts_str[9];					\
		struct tm *ts;					\
								\
		ts = localtime(&x);				\
		sprintf(ts_str, "%02d:%02d:%02d", ts->tm_hour,	\
			ts->tm_min, ts->tm_sec);		\
		ts_str;						\
	})

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

static inline long fnum_of_line(FILE *datafile)
{
	long offset = ftell(datafile);
	long length;

	fseek(datafile, 0, SEEK_END);
	length = ftell(datafile);
	fseek(datafile, offset, SEEK_SET);
	return length / DATA_ROW_WIDTH;
}

struct trade {
	char buyer[8];
	char seller[8];
	/* char market[8]; */
	char time[9];
	double price;
	long quantity;
};

#endif
