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
	int initialized;
	double ret;
	double avg_ret;
	double tolerated_loss;
	unsigned int allow_new_positions:1;
	struct timely_indicator timely[2];
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
	action_observe,
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
	unsigned int trapped;
};


extern struct trade_position my_position;
extern struct market market;
extern char orderbookId[20];
extern struct trade_const trade_constants;

int inline indicators_initialized(void);
/* void save_position(void); */
/* void restore_position(void); */
time_t get_trade(FILE *datafile, long n, struct trade *trade1);
time_t parse_time(const char *timestring);
enum order_status send_order(enum action_type action);
void set_position(const struct trade_position *position);
void discard_old_records(int size);
void analyze();
void analyzer_cleanup(void);
#endif
