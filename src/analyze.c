#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <glib.h>
#include "analyze.h"

const double fee = 198;
struct market market;
struct trade_position my_position;

const char *action_strings[number_of_action_types] = {
	"BUY", "SELL", "OBSERVE"
};

const char *trend_strings[number_of_trend_types] = {
	"up", "down", "unclear"
};

struct indicators indicators = {
	.volume = 0,
	.timely = {
		{
			.open_margin = 1.2 / 80,
		},
		{
			.open_margin = 1.5 / 80,
		},
		{
			.open_margin = 2 / 80,
		},
	},
	.trends = {
		{
			.grp_size = 15
		},
		{
			.grp_size = 20
		},
		{
			.grp_size = 25
		},
	},
	.tolerated_loss = 250
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

static time_t parse_time(const char *timestring)
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

static double straight_average(long size, FILE *datafile)
{
	int i;
	double avg;
	long total;
		
	for (i = 1, avg = 0, total = 0; i <= size; i++) {
		struct trade trade;
		fseek(datafile, -DATA_ROW_WIDTH, SEEK_CUR);
		fscanf(datafile, "%s\t%s\t%lf\t%ld",
		       trade.market, trade.time, &trade.price,
		       &trade.quantity);
		total++;
		avg += (trade.price - avg)/total;
		fseek(datafile, -DATA_ROW_WIDTH, SEEK_CUR);
	}
	return avg;
}

static void calculate_bound(FILE *datafile, long size, double *bound)
{
	long i;
		
	memset(bound, 0, 2 * sizeof(double));
	fseek(datafile, 0, SEEK_END);
	for (i = 1; i <= size; i++) {
		struct trade trade;
		fseek(datafile, -DATA_ROW_WIDTH, SEEK_CUR);
		fscanf(datafile, "%s\t%s\t%lf\t%ld",
		       trade.market, trade.time, &trade.price,
		       &trade.quantity);
		if (!bound[0])
			bound[0] = trade.price;
		else
			bound[0] = MIN(trade.price, bound[0]);
		bound[1] = MAX(trade.price, bound[1]);
		fseek(datafile, -DATA_ROW_WIDTH, SEEK_CUR);
	}
}

static enum trend get_trend(void)
{
	int i;
	enum trend trend;
	for (i = 0; i < ARRAY_SIZE(indicators.trends); i++) {
		if ((trend = indicators.trends[i].trend) != trend_unclear)
			break;
	}
	return trend;
}

#define GRP_NUM 4
static void find_trend(FILE *datafile)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(indicators.trends); i++) {
		int j;
		double avg[GRP_NUM];
		int flag;
		const double margins[GRP_NUM - 1] = {
			-0.025 / 80,
			0.05 / 80,
			0.05 / 80
		};
		if (fnum_of_line(datafile) <
		    GRP_NUM * indicators.trends[i].grp_size) {
			indicators.trends[i].trend = trend_unclear;
			continue;
		}
		fseek(datafile, 0, SEEK_END);
		for (j = GRP_NUM - 1; j >= 0; j--) {
			avg[j] = straight_average(indicators.trends[i].grp_size,
						  datafile);
		}
		for (j = 0, flag = 0; j < GRP_NUM - 1 &&
			     (flag = avg[j + 1] > avg[j] * (1 + margins[j]));
		     j++);
		if (flag) {
			indicators.trends[i].trend = trend_up;
			continue;
		}
		for (j = 0, flag = 0; j < GRP_NUM - 1 &&
			     (flag = avg[j + 1] < avg[j] * (1 - margins[j]));
		     j++);
		if (flag) {
			indicators.trends[i].trend = trend_down;
			continue;
		}
		indicators.trends[i].trend = trend_unclear;
	}
}
#undef GRP_NUM

static void calculate_indicator(FILE *datafile, time_t since,
				struct timely_indicator *ind)
{
	struct trade trade;
	const char *sincestr;
	double x;
	int i;
	long tot;

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
	x = (ind->ind[2] - ind->ind[0]) * 0.65;
	if (x * my_position.quantity - fee > 100)
		ind->open_margin = x / ind->ind[1];
	ind->available = 1;
}

static void dump_indicators(void)
{
	int i;
	struct trade *trade = (struct trade *)market.newest->data;
	printf("[%s]: %f", trade->time, trade->price);
	for (i = 0; i < ARRAY_SIZE(indicators.trends); i++) {
		printf(" %s", trend_strings[indicators.trends[i].trend]);
	}
	printf(" [%lf %lf]", indicators.bound[0], indicators.bound[1]);
	for (i = 0; i < ARRAY_SIZE(indicators.timely); i++) {
		printf(" {%lf %f}.", indicators.timely[i].ind[1],
		       indicators.timely[i].open_margin);
	}
}

static void update_indicators()
{
	GList *p;
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
	for (p = market.earliest_updated; p; p = p->next) {
		struct trade *trade = (struct trade*)p->data;
		/* average & volume */
		indicators.volume += trade->quantity;
		/* average of all trades so far */
		indicators.timely[2].ind[1] +=
			(trade->price * trade->quantity)/indicators.volume -
			indicators.timely[2].ind[1] * trade->quantity / indicators.volume;
		indicators.timely[2].ind[2] = MAX(indicators.timely[2].ind[2], trade->price);
		indicators.timely[2].ind[0] = indicators.timely[2].ind[0] ?
			MIN(indicators.timely[2].ind[0], trade->price) : trade->price;
	}
	/* There is no point comparing with the all-trades average */
	indicators.timely[2].available = 0;
	calculate_indicator(datafile, now - 20  * 60,
			    &indicators.timely[0]);
	calculate_indicator(datafile, now - 60 * 60,
			    &indicators.timely[1]);
	find_trend(datafile);
	calculate_bound(datafile, 80, indicators.bound);
#ifdef DEBUG
	indicators.allow_new_positions = 1;
#else
	indicators.allow_new_positions = strcmp(get_timestring(), "17:00:00") < 0;
#endif
	fclose(datafile);

	dump_indicators();
	/* if (my_position.status == complete || timep->tm_hour < 15) { */
	/* 	indicators.margin = 0.00625; */
	/* 	indicators.tolerated_loss = 0; */
	/* 	return; */
	/* } */
	/* diff = now - my_position.enter_time; */
	/* if (diff >= 2 * SEC_AN_HOUR && diff < 3 * SEC_AN_HOUR) { */
	/* 	indicators.margin = 0.0025; */
	/* } else if (diff >= 3 * SEC_AN_HOUR && diff < 4 * SEC_AN_HOUR) { */
	/* 	indicators.margin = 0; */
	/* 	indicators.tolerated_loss = 200; */
	/* } else if (diff >= 4 * SEC_AN_HOUR) { */
	/* 	indicators.margin = -0.0025; */
	/* 	indicators.tolerated_loss = 400; */
	/* } */
}

static double calculate_profit(double price)
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
		printf(" %s: %s.\n", __func__, action_strings[action]);
		my_position.status = incomplete;
		my_position.price = price;
		my_position.enter_time = parse_time(trade->time);
		break;
	case 0b001: /*buy-and-sell, complete, sell */
	case 0b010: /*buy-and-sell, incomplete, buy */
	case 0b100: /*sell-and-buy, complete, buy */
	case 0b111: /*sell-and-buy, incomplete, sell */
		printf(" %s: Rejected.\n", __func__);
		break;
	case 0b011: /*buy-and-sell, incomplete, sell */
	case 0b110: /*sell-and-buy, incomplete, buy */
		my_position.status = complete;
		printf(" %s: %s.\n", __func__, action_strings[action]);
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

/* static enum action_type terminator(struct conclusion *conclusion) */
/* { */
/* 	int size = 360; */
/* 	double avg; */
/* 	double latest; */
/* 	double margin = 0.3 / 80; */

/* 	conclusion->analyzer = __func__; */
/* 	if (market.trades_count < size || */
/* 	    my_position.status == complete) */
/* 		return conclusion->action = action_unclear; */

/* 	latest = ((struct trade *)market.newest->data)->price; */
/* 	avg = weighted_average(market.trades_count - size, size); */

/* 	if (my_position.mode == sell_and_buy &&  */
/* 	    latest > my_position.price * (1 + margin)) { */
/* 		return conclusion->action = action_buy; */
/* 	} */
/* 	if (my_position.mode == buy_and_sell && */
/* 	    latest < my_position.price * (1 - margin)) */
/* 		return conclusion->action = action_sell; */
/* 	return conclusion->action = action_unclear; */
/* } */



/* static enum action_type clear_trend(struct conclusion *conclusion) */
/* { */
/* #define GRP_NUM 4 */
/* 	int group_size = 15; */
/* 	int i; */
/* 	int flag; */
/* 	double avg[GRP_NUM]; */
/* 	int offset = market.trades_count - GRP_NUM * group_size; */
/* 	const double margins[GRP_NUM - 1] = { */
/* 		-0.025 / 80, */
/* 		0.05 / 80, */
/* 		0.05 / 80 */
/* 	}; */
/* 	const double margin = 0.5 / 80; */
/* 	double latest = ((struct trade *)market.newest->data)->price; */
/* 	double regavg = 82; */

/* 	conclusion->analyzer = __func__; */
/* 	for (i = 0; i < GRP_NUM; i++) */
/* 		avg[i] = straight_average(offset + i * group_size, group_size); */
/* 	for (i = 0, flag = 0; */
/* 	     i < GRP_NUM - 1 && (flag = avg[i + 1] > avg[i] * (1 + margins[i])); */
/* 	     i++); */

/* 	if (flag && latest < indicators.ind[2] * (1 - margin) && */
/* 	    (regavg < 0 || latest < regavg)) { */
/* 		return conclusion->action = action_buy; */
/* 	} */
/* 	else if (flag && my_position.status == complete) */
/* 		return conclusion->action = action_observe; */
/* 	for (i = 0, flag = 0; */
/* 	     i < GRP_NUM - 1 && (flag = avg[i + 1] < avg[i] * (1 - margins[i])); */
/* 	     i++); */
/* 	if (flag && latest > indicators.ind[0] * (1 + margin) && */
/* 	    (regavg < 0 || latest > regavg)) { */
/* 		return conclusion->action = action_sell; */
/* 	} */
/* 	else if (flag && my_position.status == complete) */
/* 		return conclusion->action = action_observe; */
/* 	/\* Situation unclear *\/ */
/* 	return action_unclear; */
/* #undef GRP_NUM */
/* } */

/* static enum action_type local_moving(struct conclusion *conclusion) */
/* { */
/* 	const int span = MIN_ANALYSIS_SIZE; */
/* 	int offset = market.trades_count - span; */
/* 	double latest = ((struct trade *)market.newest->data)->price; */
/* 	double average = weighted_average(offset, span); */
/* 	const double margin = 0.35 / 80; */

/* 	conclusion->analyzer = __func__; */
/* 	conclusion->action = action_unclear; */
/* 	if (latest < average * (1 - margin)) */
/* 		conclusion->action = action_buy; */
/* 	else if (latest > average * (1 + margin)) */
/* 		conclusion->action = action_sell; */
/* 	return conclusion->action; */
/* } */


/* static enum action_type happy_ending(struct conclusion *conclusion) */
/* { */
/* 	double latest = ((struct trade *)market.newest->data)->price; */
/* 	double margin = 0.00625; */

/* 	conclusion->analyzer = __func__; */
/* 	conclusion->action = action_unclear; */
/* 	if (my_position.status == complete) */
/* 		return conclusion->action; */
/* 	if (my_position.mode == sell_and_buy && */
/* 	    latest <=  my_position.price * (1 - margin)) { */
/* 		conclusion->action = action_buy; */
/* 	} else if (my_position.mode == buy_and_sell && */
/* 		   latest >=  my_position.price * (1 + margin)) { */
/* 		conclusion->action = action_sell; */
/* 	} */
/* 	return conclusion->action; */
/* } */

static enum action_type price_comparer(double latest, int index)
{
	enum action_type action;
	struct timely_indicator *ind = indicators.timely + index;
	double lim;

	if (!ind->available)
		return action_observe;
	lim = my_position.mode == buy_and_sell ?
		ind->ind[1] * (1 - ind->open_margin) :
		ind->ind[1] * (1 + ind->open_margin);

	if (my_position.mode == buy_and_sell && latest < lim)
		action = action_buy;
	else if (my_position.mode == sell_and_buy && latest > lim)
		action = action_sell;
	else
		action = action_observe;

	if (action != action_observe)
		printf(" %s-%d:", __func__, index);
	return action;
}

static enum action_type trend_follower(double price)
{
	enum action_type action = action_observe;
	enum trend trend = get_trend();
	double lim, margin = 0.002;

	if (trend == trend_unclear)
		return action_observe;
	lim = trend == trend_up ?
		indicators.bound[0] * (1 + margin):
		indicators.bound[1] * (1 - margin);
	switch (trend) {
	case trend_up:
		if (my_position.mode == buy_and_sell && price < lim)
			action = action_buy;
		break;
	case trend_down:
		if (my_position.mode == sell_and_buy && price > lim)
			action = action_sell;
		break;
	default:;
	}
	if (action != action_observe)
		printf(" %s:", __func__);
	return action;
}

void analyze()
{
	enum action_type action = action_observe;
	enum trend trend;
	double happy_margin = 0.6 / 80;
	struct trade *trade = (struct trade *)market.newest->data;
	double latest = trade->price;

	update_indicators();
	trend = get_trend();
	switch (my_position.status) {
	case incomplete: {
		double profit = calculate_profit(latest);
		time_t t = parse_time(trade->time);
		switch (my_position.mode) {
		case sell_and_buy:
			if (trend == trend_up || (trend == trend_unclear &&
						  latest < my_position.price
						  * (1 - happy_margin))) {
				action = action_buy;
			} else {
				action = action_observe;
			}
			break;
		case buy_and_sell:
			if (trend == trend_down || (trend == trend_unclear &&
						    latest > my_position.price
						    * (1 + happy_margin))) {
				action = action_sell;
			} else {
				action = action_observe;
			}
			break;
		}
		if (profit > 0 && action != action_observe)
			break;
		if (t - my_position.enter_time < 90 * 60 && profit < 0) {
			action = action_observe;
			break;
		}
		if (t - my_position.enter_time > 90 * 60) {
			if (profit > 0 || -profit < indicators.tolerated_loss) {
				action = my_position.mode == buy_and_sell ?
					action_sell : action_buy;
			} else {
				action = action_observe;
			}
		}
		break;
	}
	case complete: {
		int i;
		/* if (strcmp(trade->time, "09:10:00") < 0) */
		/* 	break; */
		action = trend_follower(latest);
		if (action != action_observe)
			break;
		for (i = 0; i < ARRAY_SIZE(indicators.timely) &&
			     action == action_observe; i++) {
			action = price_comparer(latest, i);
		}
	}
	}
	if (action == action_buy || action == action_sell)
		execute(action);
	else
		printf(" Observe.\n");
	discard_old_records(market.trades_count - MAX_TRADES_COUNT);
	fflush(stdout);
}

