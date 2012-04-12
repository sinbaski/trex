#ifndef INTRADAY_ANALYZE_H
#define INTRADAY_ANALYZE_H
#include <mysql.h>
#include <glib.h>
#include "trade.h"

#define ORDER_DEPTH 5
/* Most likely 80 */
#define MIN_ANALYSIS_SIZE 800
#define DATA_SIZE 3200
#define MAX_TRADES_COUNT 80

#define DATA_ROW_WIDTH 32

struct order {
	float price;
	long quantity;
};

struct order_depth {
	/* in the descending order of the price */
	struct order bid[ORDER_DEPTH];
	/* in the ascending order of the price */
	struct order ask[ORDER_DEPTH];
};

struct market {
	struct order_depth order_depth;
	int new_trades;
	GList *trades;
};

struct price_interval {
	char price[20];
	long n;
};

struct timely_indicator {
	/* lowest average highest */
	double ind[3];
	int available;
	/* last excluded line */
	long mark;
	long sample_size;
	int period;
	/*A list of struct price_interval */
	GList *probdist;
};

struct indicators {
	double tolerated_loss;
	unsigned int allow_new_positions:1;
};

struct trade_const {
	const double dprice;
	const char *dpricestr;
	const int dpricepcr;
};

enum trade_mode {
	buy_and_sell = 0,
	sell_and_buy
};

enum trade_status {
	complete = 0,
	incomplete
};

enum action_type {
	action_buy = 0,
	action_sell,
	action_none,
	number_of_action_types,
};

enum order_status {
	order_executed = 0,
	order_killed,
	order_failed,
};

struct trade_position {
	enum trade_mode mode;
	enum trade_status status;
	double price;
	/* The number of shares we trade */
	long quantity;
};

struct stock_info {
	const char *name;
	const char *dataid;
	const char *orderid;
};

extern struct trade_position my_position;
extern struct market market;
extern enum trade_status enter_status;
extern char orderbookId[20];
extern int do_trade;
extern char todays_date[11];
extern MYSQL *mysqldb;
extern const struct stock_info stockinfo[];
extern struct trade_const trade_constants;

int inline indicators_initialized(void);
/* void save_position(void); */
/* void restore_position(void); */
int get_num_records(const char *date);
time_t get_trade(MYSQL *db, long n, struct trade *trade1);
time_t parse_time(const char *timestring);
enum order_status send_order(enum action_type action);
void set_position(const struct trade_position *position);
void discard_old_records(int size);
void analyze(void);
void analyzer_cleanup(void);
const struct stock_info *get_stock_info(const char *id);

#define get_tbl_name()							\
	({								\
		int i;							\
		char tbl_name[50];					\
		const struct stock_info *info = get_stock_info(orderbookId); \
		strncpy(tbl_name, info->name, sizeof(tbl_name));	\
		for (i = 0; i < sizeof(tbl_name); i++) {		\
			if (tbl_name[i] == ' ') tbl_name[i] = '_';	\
		}							\
		tbl_name;						\
	})

#endif
