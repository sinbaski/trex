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
	action_sell = -1,
	action_none = 0,
	action_buy = 1,
	number_of_action_types = 3
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
	double quota;
	long quantity;
	char time[9];
};

struct stock_info {
	char name[65];
	char tbl_name[65];
	/* dataid is the orderbookId */
	char dataid[7];
	char orderid[7];
};

struct trade_flags {
	int do_trade:1;
	int allow_new_entry:1;
};

struct trade_instruction {
	enum action_type action;
	double price;
	char expiration[9];
};

extern struct trade_position my_position;
extern struct trade_flags my_flags;
extern struct trade_instruction latest_instruction;
extern struct market mkt;
extern enum trade_status enter_status;
extern struct stock_info stockinfo;
extern char todays_date[11];
extern MYSQL *mysqldb;
extern struct trade_const trade_constants;

int inline indicators_initialized(void);
/* void save_position(void); */
/* void restore_position(void); */
int get_num_records(const char *date);
time_t get_trade(MYSQL *db, long n, struct trade *trade1);
time_t parse_time(const char *timestring);
enum order_status send_order(enum action_type action, const char *price);
void discard_old_records(int size);
enum order_status execute(enum action_type action, double price);
void analyze(void);
void analyzer_cleanup(void);
struct stock_info *get_stock_info(const char *id, struct stock_info *);
#endif
