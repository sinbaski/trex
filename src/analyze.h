#ifndef INTRADAY_ANALYZE_H
#define INTRADAY_ANALYZE_H
#include <glib.h>
#include "trade.h"

#define ORDER_DEPTH 5
/* Most likely 80 */
#define MIN_ANALYSIS_SIZE 80
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
	GList *trades;
	GList *newest;
	GList *earliest_updated;
	long trades_count;
};

enum trend {
	trend_up = 0,
	trend_down,
	trend_unclear,
	number_of_trend_types
};

struct trend_indicator {
	enum trend trend;
	int grp_size;
};

struct timely_indicator {
	/* lowest average highest */
	double ind[3];
	int available;
	double open_margin;
};

struct indicators {
	long volume;
	/* lowst highest of the last batch */
	double bound[2];
	double tolerated_loss;
	unsigned int allow_new_positions:1;
	struct timely_indicator timely[3];
	struct trend_indicator trends[3];
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
	action_observe,
	number_of_action_types,
};

struct trade_position {
	enum trade_mode mode;
	enum trade_status status;
	double price;
	long quantity;
	/* The time at which the position is entered */
	time_t enter_time;
};

char orderbookId[20];

time_t parse_time(const char *timestring);
void set_position(const struct trade_position *position);
void discard_old_records(int size);
int trade_equal(const struct trade *t1, const struct trade *t2);
void analyze();
#endif
