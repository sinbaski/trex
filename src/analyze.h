#ifndef INTRADAY_ANALYZE_H
#define INTRADAY_ANALYZE_H

#include <glib.h>

#define ORDER_DEPTH 5
/* Most likely 80 */
#define BATCH_SIZE 80
#define SEC_AN_HOUR 3600

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

struct trade {
	char market[10];
	char time[9];
	double price;
	long quantity;
};

struct market {
	struct order_depth order_depth;
	GList *trades;
	GList *newest;
	GList *earliest_updated;
	long trades_count;
};

struct indicators {
	double average;
	long volume;
	double margin;
	double tolerated_loss;
	int allow_new_positions:1;
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
	action_unclear,
	number_of_action_types
};

struct trade_position {
	enum trade_mode mode;
	enum trade_status status;
	double price;
	long quantity;
	/* The time at which the position is entered */
	time_t enter_time;
};

void set_position(const struct trade_position *position);
int trade_equal(const struct trade *t1, const struct trade *t2);
void analyze();
#endif
