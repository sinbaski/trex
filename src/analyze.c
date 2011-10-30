#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <glib.h>
#include "analyze.h"

extern int send_order(enum action_type action);

/* 2 hours */
const double fee = 198;
struct market market;
struct trade_position my_position;

const char *action_strings[number_of_action_types] = {
	"BUY", "SELL", "OBSERVE"
};

struct indicators indicators = {
	.volume = 0,
	.timely = {
		{
			.margin = 1.2 / 80,
		},
		{
			.margin = 1.2 / 80,
		},
		{
			.margin = 1.2 / 80,
		},
	},
	.tolerated_loss = 250
};

struct {
	const float min_profit_open;
	const float min_profit_close;
} policy = {
	.min_profit_open = 350,
	.min_profit_close = 250
};

int indicators_initialized(void)
{
	return indicators.volume > 0;
}

void set_position(const struct trade_position *position)
{
	my_position.mode = position->mode;
	my_position.status = position->status;
	my_position.price = position->price;
	my_position.enter_time = position->enter_time;
	my_position.quantity = position->quantity;
}

void save_indicators(void)
{
	FILE *fp = fopen(get_filename("indicators", ".ind"), "w");
	fwrite(&indicators, sizeof(indicators), 1, fp);
	fclose(fp);
}

void restore_indicators(void)
{
	FILE *fp = fopen(get_filename("indicators", ".ind"), "r");
	if (!fp)
		return;
	fread(&indicators, sizeof(indicators), 1, fp);
	fclose(fp);
}

/* static double weighted_average(int index, int size) */
/* { */
/* 	GList *p = g_list_nth(market.trades, index); */
/* 	int i; */
/* 	double avg; */
/* 	long total; */

/* 	for (i = 0, avg = 0, total = 0; i < size; i++) { */
/* 		struct trade *trade = (struct trade *)p->data; */
/* 		total += trade->quantity; */
/* 		avg += (trade->price * trade->quantity)/total - */
/* 			avg * trade->quantity / total; */
/* 	} */

/* 	return avg; */
/* } */

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

static time_t get_trade(FILE *datafile, long n, struct trade *trade1)
{
	long offset = ftell(datafile);
	struct trade trade2;
	struct trade *trade = trade1 ? trade1 : &trade2;

	fseek(datafile, n * DATA_ROW_WIDTH, SEEK_SET);
	fscanf(datafile, "%s\t%s\t%lf\t%ld",
	       trade->market, trade->time, &trade->price,
	       &trade->quantity);
	fseek(datafile, offset, SEEK_SET);
	return parse_time(trade->time);
}

static long fnum_of_line(FILE *datafile)
{
	long offset = ftell(datafile);
	long length;

	fseek(datafile, 0, SEEK_END);
	length = ftell(datafile);
	fseek(datafile, offset, SEEK_SET);
	return length / DATA_ROW_WIDTH;
}

static int is_trapped(void)
{
	int trapped;
	if (my_position.status != incomplete)
		return 0;
	if (my_position.mode == buy_and_sell)
		trapped = indicators.timely[2].ind[1] < my_position.price;
	else
		trapped = indicators.timely[2].ind[1] > my_position.price;
	return trapped;
}

static double cal_der_sum(FILE *datafile, long n)
{
	long i;
	double sum;
	struct trade trade;

	assert(fnum_of_line(datafile) >= n);
	for (i = 1, sum = 0; i <= n; i++) {
		double p1, p2;
		fseek(datafile, -i * DATA_ROW_WIDTH, SEEK_END);
		fscanf(datafile, "%s\t%s\t%lf\t%ld",
		       trade.market, trade.time, &trade.price,
		       &trade.quantity);
		if (i == 1)
			p2 = trade.price;
		else {
			p1 = trade.price;
			sum += (p2 - p1)/p1;
			p2 = p1;
		}
	}
	return sum;
}

static void cal_indicator(FILE *datafile, time_t since, int idx)
{
	struct trade trade;
	const char *sincestr;
	double x, y;
	int i;
	long tot;
	/* double max_margin[] = { */
	/* 	0.008, 0.01, 0.012 */
	/* }; */
	struct timely_indicator *ind = indicators.timely + idx;

	if (get_trade(datafile, 0, NULL) > since ||
	    get_trade(datafile, fnum_of_line(datafile) - 1,
		      NULL) < since) {
		ind->available = 0;
		return;
	}
	sincestr = make_timestring(since);
	ind->ind[1] = ind->ind[2] = ind->ind[0] = 0;
	tot = 0;
	for (i = 1; 1; i++) {
		fseek(datafile, -i * DATA_ROW_WIDTH, SEEK_END);
		fscanf(datafile, "%s\t%s\t%lf\t%ld",
		       trade.market, trade.time, &trade.price,
		       &trade.quantity);
		if (strcmp(trade.time, sincestr) < 0)
			break;
		tot += trade.quantity;
		ind->ind[1] += (trade.price * trade.quantity)/tot
			- ind->ind[1] * trade.quantity / tot;
		ind->ind[2] = MAX(ind->ind[2], trade.price);
		ind->ind[0] = ind->ind[0] ? MIN(ind->ind[0], trade.price) :
			trade.price;
		/* Break if we are at the beginning of the file */
		if (ftell(datafile) == 0)
			break;
	}
	/* x = MIN(x, max_margin[idx]); */
	if (my_position.status == complete) {
		x = (ind->ind[2] - ind->ind[0]) * 0.5 * 0.8 / ind->ind[1];
		y = (policy.min_profit_open + fee) / my_position.quantity
			/ ind->ind[1];
		ind->margin = MAX(x, y);
	} else {
		x = (ind->ind[2] - ind->ind[0]) * 0.5 * 0.6 / ind->ind[1];
		ind->margin = x;
	}
	ind->available = 1;
}

static void dump_indicators(void)
{
	int i;
	struct trade *trade = (struct trade *)market.newest->data;
	printf("[%s]: %f %lf", trade->time, trade->price, indicators.der);
	for (i = 0; i < ARRAY_SIZE(indicators.timely); i++) {
		printf(" {%lf %f}.", indicators.timely[i].ind[1],
		       indicators.timely[i].margin);
	}
}

static void update_indicators()
{
	time_t now;
	FILE *datafile = fopen(get_filename("records", ".dat"), "r");
	int i;
	if (!datafile) {
		fprintf(stderr, "%s: Failed to open datafile. Error %d.\n",
			__func__, errno);
		return;
	}
	now = parse_time(((struct trade *)market.newest->data)->time);
	if (!indicators.volume) {
		for (i = 0; i < ARRAY_SIZE(indicators.timely); i++) {
			int j;
			for (j = 0; j < ARRAY_SIZE(indicators.timely[i].ind);
			     j++) {
				indicators.timely[i].ind[j] = 0;
			}
			indicators.timely[i].available = 0;
		}
	}
	cal_indicator(datafile, now - 20 * 60, 0);
	cal_indicator(datafile, now - 45 * 60, 1);
	cal_indicator(datafile, now - 90 * 60, 2);
	indicators.der = cal_der_sum(datafile, 40);
#ifdef DEBUG
	indicators.allow_new_positions = 1;
#else
	indicators.allow_new_positions = strcmp(get_timestring(), "17:00:00") < 0;
#endif
	fclose(datafile);

	dump_indicators();
}

static double cal_profit(double price)
{
	double diff;
	switch (my_position.mode)
	{
	case sell_and_buy:
		diff = my_position.price - price;
		break;
	case buy_and_sell:
		diff = price - my_position.price;
		break;
	}
	return diff * my_position.quantity - fee;
}

static void execute(enum action_type action)
{
	struct trade *trade = (struct trade *)market.newest->data;
	double price = trade->price;

	switch (my_position.mode << 2 | my_position.status << 1 | action) {
	case 0b000: /*buy-and-sell, complete, buy */
	case 0b101: /*sell-and-buy, complete, sell */
		if (!indicators.allow_new_positions) {
			printf(" %s: Disallowed.\n", __func__);
			break;
		}
		if (send_order(action) == 0) {
			my_position.status = incomplete;
			my_position.price = price;
			my_position.enter_time = parse_time(trade->time);
			printf(" %s: %s.\n", __func__, action_strings[action]);
		} else
			printf(" %s: %s failed.\n", __func__,
			       action_strings[action]);
		break;
	case 0b001: /*buy-and-sell, complete, sell */
	case 0b010: /*buy-and-sell, incomplete, buy */
	case 0b100: /*sell-and-buy, complete, buy */
	case 0b111: /*sell-and-buy, incomplete, sell */
		printf(" %s: Rejected.\n", __func__);
		break;
	case 0b011: /*buy-and-sell, incomplete, sell */
	case 0b110: /*sell-and-buy, incomplete, buy */
		if (send_order(action) == 0) {
			my_position.status = complete;
			printf(" %s: %s.\n", __func__,
			       action_strings[action]);
		} else
			printf(" %s: %s failed.\n", __func__,
			       action_strings[action]);
	}
}

int trade_equal(const struct trade *t1, const struct trade *t2)
{
	return 	strcmp(t1->market, t2->market) == 0 &&
		strcmp(t1->time, t2->time) == 0 &&
		t1->price - t2->price > -0.01 &&
		t1->price - t2->price < 0.01 &&
		t1->quantity == t2->quantity;
}

static enum action_type price_comparer(double latest, int index)
{
	enum action_type action = action_observe;
	struct timely_indicator *ind = indicators.timely + index;
	double lim, profit;

	if (!ind->available)
		return action_observe;
	switch (my_position.mode << 1 | my_position.status) {
	case 0b00:
		lim = ind->ind[1] * (1 - ind->margin);
		if (latest <= lim && indicators.der >= 0.0008)
			action = action_buy;
		break;
	case 0b10:
		lim = ind->ind[1] * (1 + ind->margin);
		if (latest >= lim && indicators.der <= -0.0008)
			action = action_sell;
		break;
	case 0b01:
		lim = ind->ind[1] * (1 + ind->margin);
		profit = cal_profit(latest);
		if (indicators.der > 0.0008)
			action = action_observe;
		else if (latest > lim)
			action = action_sell;
		else if (profit > policy.min_profit_close)
			action = action_sell;
		break;
	case 0b11:
		lim = ind->ind[1] * (1 - ind->margin);
		profit = cal_profit(latest);
		if (indicators.der < -0.0008)
			action = action_observe;
		else if (latest < lim)
			action = action_buy;
		else if (profit > policy.min_profit_close)
			action = action_buy;
		break;
	}
	if (action != action_observe)
		printf(" %s-%d:", __func__, index);
	return action;
}

void analyze()
{
	enum action_type action = action_observe;
	struct trade *trade = (struct trade *)market.newest->data;
	double latest = trade->price;
	int i;

	update_indicators();
	for (i = 0; i < ARRAY_SIZE(indicators.timely) &&
		     action == action_observe; i++) {
		action = price_comparer(latest, i);
	}

	switch (my_position.status) {
	case incomplete: {
		double profit = cal_profit(latest);
		int trapped = is_trapped();
		if (profit > 0 && action != action_observe)
			break;
		if (!trapped && profit < 0) {
			action = action_observe;
			break;
		}
		if (trapped) {
			if (profit > 0 || -profit < indicators.tolerated_loss) {
				action = my_position.mode == buy_and_sell ?
					action_sell : action_buy;
			} else {
				action = action_observe;
			}
		}
		break;
	}
	case complete:;
	}
	if (action != action_observe)
		execute(action);
	else
		printf(" Observe.\n");
	discard_old_records(market.trades_count - MAX_TRADES_COUNT);
	fflush(stdout);
}

