#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
#include <glib.h>
#include "analyze.h"
#include "utilities.h"

#define MIN_SAMPLE_SIZE 400
#define price2str(price, str)						\
	({								\
		sprintf(str, "%.*f", trade_constants.dpricepcr, price); \
	})

const double fee = 198;
struct market market;
struct trade_position my_position;

const char *action_strings[number_of_action_types] = {
	"BUY", "SELL", "OBSERVE"
};

struct indicators indicators = {
	.initialized = 0,
	.timely = {
		{
			.available = 0,
			.probdist = NULL,
			.period = 20 * 60,
		},
		{
			.available = 0,
			.probdist = NULL,
			.period = 40 * 60,
		},
		{
			.available = 0,
			.probdist = NULL,
			.period = 60 * 60,
		},
	},
	.tolerated_loss = 250,
	.allow_new_positions = 1,
	.ret = 0,
	.avg_ret = 0,
};

struct trade_const trade_constants = {
	.dpricestr = "0.0001",
	/* 4 digits after the decimal point */
	.dpricepcr = 4
};

struct {
	const float min_open_profit;
	const float min_close_profit;
} policy = {
	.min_open_profit = 100,
	.min_close_profit = 100
};

int inline indicators_initialized(void)
{
	return indicators.initialized;
}

void set_position(const struct trade_position *position)
{
	my_position.mode = position->mode;
	my_position.price = position->price;
	my_position.quantity = position->quantity;
}

static inline double get_return(void)
{
	return 0.6 * indicators.ret + 0.4 * indicators.avg_ret;
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

time_t get_trade(FILE *datafile, long n, struct trade *trade1)
{
	long offset = ftell(datafile);
	struct trade trade2;
	struct trade *trade = trade1 ? trade1 : &trade2;

	assert(n >= 0 && n < fnum_of_line(datafile));
	fseek(datafile, n * DATA_ROW_WIDTH, SEEK_SET);
	fscanf(datafile, "%s\t%s\t%lf\t%ld",
	       trade->market, trade->time, &trade->price,
	       &trade->quantity);
	fseek(datafile, offset, SEEK_SET);
	return parse_time(trade->time);
}

static void free_price_interval(void *p)
{
	g_slice_free(struct price_interval, p);
}

static double cal_ret(FILE *datafile, long n1, long n2)
{
	long l = ftell(datafile);
	struct trade trade1, trade2;

	assert(n1 >= 0 && n1 < n2 && fnum_of_line(datafile) >= n2);
	fseek(datafile, -(n1 + 1) * DATA_ROW_WIDTH, SEEK_END);
	fscanf(datafile, "%s\t%s\t%lf\t%ld",
	       trade1.market, trade1.time, &trade1.price,
	       &trade1.quantity);
	fseek(datafile, -n2 * DATA_ROW_WIDTH, SEEK_END);
	fscanf(datafile, "%s\t%s\t%lf\t%ld",
	       trade2.market, trade2.time, &trade2.price,
	       &trade2.quantity);
	fseek(datafile, l, SEEK_SET);
	return (trade1.price - trade2.price)/trade2.price;
}

static double cal_avg_ret(FILE *datafile, long n, long m)
{
	long i;
	double sum;

	assert(n > 0 && m > 0 && fnum_of_line(datafile) >= n * m - (n - 1));
	for (i = 0, sum = 0; i < n; i++) {
		/* sum += cal_ret(datafile, i * (m - 1), (i + 1) * m - i); */
		sum += cal_ret(datafile, i * m - i, (i + 1) * m - i);
	}
	return sum / n;
}

static struct price_interval *new_price_interval(const char *price, long n)
{
	struct price_interval *x;
	x = g_slice_new(struct price_interval);
	strcpy(x->price, price);
	x->n = n;
	return x;
}

static int pricecmp(const char *pstr1, const char *pstr2)
{
	int l1, l2;
	if ((l1 = strlen(pstr1)) != (l2 = strlen(pstr2)))
		return l1 - l2;
	else
		return strcmp(pstr1, pstr2);
}

static void update_probdist(int idx, const struct trade *trade, int plus)
/* static void update_probdist(GList **list, const struct trade *trade, int plus) */
{
	GList *node, *node2 = NULL;
	char pstr[20];
	GList **list = &indicators.timely[idx].probdist;
	static GList *last_match[ARRAY_SIZE(indicators.timely)] = {
		NULL, NULL, NULL
	};

	assert(idx >= 0 && idx < ARRAY_SIZE(indicators.timely));
	price2str(trade->price, pstr);
	node = last_match[idx] != NULL ? last_match[idx] : *list;
	while (node != NULL) {
		struct price_interval *itv =
			(struct price_interval *)node->data;
		int cmp = pricecmp(pstr, itv->price);
		struct price_interval *itv2, *itv3;
		int cmp2;
		if (cmp < 0) {
			if (node == *list)
				goto label1;
			itv2 = (struct price_interval *)node->prev->data;
			cmp2 = pricecmp(pstr, itv2->price);
			if (cmp2 > 0) {
				itv3 = new_price_interval(
					pstr, trade->quantity);
				*list = g_list_insert_before(*list, node, itv3);
				last_match[idx] = node->prev;
				return;
			} else if (cmp2 == 0) {
				itv2->n += trade->quantity;
				last_match[idx] = node->prev;
				return;
			}
		label1:
			node2 = node;
			node = node->prev;
		} else if (cmp == 0) {
			if (plus)
				itv->n += trade->quantity;
			else {
				itv->n -= trade->quantity;
				if (itv->n == 0) {
					*list = g_list_remove_link(*list, node);
					g_list_free_full(
						node, free_price_interval);
				}
			}
			last_match[idx] = node;
			return;
		} else if (cmp > 0) {
			if (node->next == NULL) {
				goto label2;
			}
			itv2 = (struct price_interval *)node->next->data;
			cmp2 = pricecmp(pstr, itv2->price);
			if (cmp2 < 0) {
				itv3 = new_price_interval(
					pstr, trade->quantity);
				*list = g_list_insert_before(*list, node->next,
							     itv3);
				last_match[idx] = node->next;
				return;
			} else if (cmp2 == 0) {
				itv2->n += trade->quantity;
				last_match[idx] = node->next;
				return;
			}
		label2:
			node2 = node;
			node = node->next;
		}
	}
	assert(plus);
	if (*list == NULL || node2 == *list) {
		*list = g_list_prepend(*list, new_price_interval(
					      pstr, trade->quantity));
		last_match[idx] = *list;
	} else {
		*list = g_list_append(*list, new_price_interval(
					      pstr, trade->quantity));
		last_match[idx] = node2->next;
	}
}

static void cal_indicator(FILE *datafile, time_t since, int idx)
{
	struct trade trade;
	const char *sincestr;
	/* double x, y; */
	long i, n;
	int first_run;

	/* double percentage[] = { */
	/* 	0.5, 0.7, 0.9 */
	/* }; */
	struct timely_indicator *ind = indicators.timely + idx;

	if (get_trade(datafile, 0, NULL) > since ||
	    get_trade(datafile, fnum_of_line(datafile) - 1,
		      NULL) < since) {
		ind->available = 0;
		return;
	}
	first_run = g_list_length(ind->probdist) == 0;
	sincestr = make_timestring(since);
	if (first_run) {
		for (i = 1; 1; i++) {
			fseek(datafile, -i * DATA_ROW_WIDTH, SEEK_END);
			fscanf(datafile, "%s\t%s\t%lf\t%ld",
			       trade.market, trade.time, &trade.price,
			       &trade.quantity);
			if (strcmp(trade.time, sincestr) < 0) {
				i--;
				break;
			}
			update_probdist(idx, &trade, 1);
			if (ftell(datafile) == DATA_ROW_WIDTH)
				break;
		}
		ind->mark = fnum_of_line(datafile) - i + 1;
		ind->sample_size = i;
	} else {
		struct trade *p;
		n = g_list_length(market.trades);
		for (i = g_list_length(market.trades) - market.new_trades;
		     i < n; i++) {
			p = (struct trade *)
				g_list_nth_data(market.trades, i);
			update_probdist(idx, p, 1);
		}
		ind->sample_size += market.new_trades;
	}
	if (first_run) {
		goto end;
	}
	for (i = ind->mark; 1; i++) {
		fseek(datafile, i * DATA_ROW_WIDTH, SEEK_SET);
		fscanf(datafile, "%s\t%s\t%lf\t%ld",
		       trade.market, trade.time, &trade.price,
		       &trade.quantity);
		if (strcmp(trade.time, sincestr) < 0) {
			update_probdist(idx, &trade, 0);
		} else {
			break;
		}
	}
	ind->mark = i;
end:
	ind->available = ind->sample_size >= MIN_SAMPLE_SIZE;
}

static void dump_indicators(void)
{
	struct trade *trade = (struct trade *)g_list_last(market.trades)->data;
	printf("[%s]: p=%.2f, r40=% e, r400=% e, r=% e;\t", trade->time, trade->price,
	       indicators.ret, indicators.avg_ret, get_return());
}

/* static void dump_probdist(void) */
/* { */
/* 	struct trade *trade = (struct trade *)g_list_last(market.trades)->data; */
/* 	FILE *fp = fopen(get_filename("probdist", ".txt"), "a"); */
/* 	GList *node; */
/* 	int i; */

/* 	fprintf(fp, "[%s]:\n", trade->time); */
/* 	for (i = 0; i < ARRAY_SIZE(indicators.timely); i++) { */
/* 		struct timely_indicator *ind = indicators.timely + i; */
/* 		fprintf(fp, "Prob%d: ", i); */
/* 		for (node = ind->probdist; node; node = node->next) { */
/* 			struct price_interval *val = */
/* 				(struct price_interval *)node->data; */
/* 			fprintf(fp, "%5.2lf,%ld   ", val->p1, val->n); */

/* 		} */
/* 		fprintf(fp, "\n"); */
/* 	} */
/* 	fclose(fp); */
/* } */

static void update_indicators()
{
	time_t now;
	FILE *datafile = fopen(get_filename("records", ".dat"), "r");
	const long grp_size = 40;
	const long grp_num = 10;
	int i;
	struct trade *trade= (struct trade *)g_list_last(market.trades)->data;
	if (!datafile) {
		fprintf(stderr, "%s: Failed to open datafile. Error %d.\n",
			__func__, errno);
		return;
	}
	now = parse_time(trade->time);
	if (!indicators.initialized)
		indicators.initialized = 1;
	for (i = 0; i < ARRAY_SIZE(indicators.timely); i++)
		cal_indicator(datafile, now - indicators.timely[i].period, i);
#if CURFEW_AFT_5
	indicators.allow_new_positions =
		strcmp(get_timestring(), "17:00:00") < 0;
#else
	indicators.allow_new_positions = 1;
#endif
	if (fnum_of_line(datafile) < grp_num * grp_size) {
		fclose(datafile);
		return;
	}
	indicators.ret = cal_ret(datafile, 0, grp_size);
	if (fnum_of_line(datafile) >= grp_num * grp_size - (grp_num - 1))
		indicators.avg_ret = cal_avg_ret(datafile, grp_num, grp_size);
	fclose(datafile);
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
	struct trade *trade = (struct trade *)g_list_last(market.trades)->data;
	double price = trade->price;

	switch (my_position.mode << 2 | my_position.status << 1 | action) {
	case 0b000: /*buy-and-sell, complete, buy */
	case 0b101: /*sell-and-buy, complete, sell */
		if (!indicators.allow_new_positions) {
			printf(" %s: Disallowed.\n", __func__);
			break;
		}
		if (send_order(action) == order_executed) {
			my_position.status = incomplete;
			my_position.price = price;
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

/*
  section < 0: percentage of trades whose prices are lower
  than the given;

  section > 0: percentage of trades whose prices are higher
  than the given;
 */
static double get_price_prob(int section, double price, int idx)
{
	struct timely_indicator *ind = indicators.timely + idx;
	GList *node;
	long sum, n;

	assert(idx < ARRAY_SIZE(indicators.timely));
	assert(ind->available);
	assert(section != 0);

	if (!ind->available)
		return -1;
	for (node = ind->probdist, sum = n = 0; node; node = node->next) {
		struct price_interval *intvl =
			(struct price_interval *)node->data;
		char strprice[20];
		int cmp;

		price2str(price, strprice);
		cmp = pricecmp(intvl->price, strprice);
		sum += intvl->n;
		if (cmp < 0 && section < 0)
			n += intvl->n;
		else if (cmp > 0 && section > 0)
			n += intvl->n;
	}
	return (double)n/sum;
}

static int is_trapped(void)
{
	double prob;
	enum {
		lower_than = -1,
		higher_than = 1
	};

	if (my_position.status != incomplete ||
	    !indicators.timely[2].available)
		return 0;
	if (my_position.mode == buy_and_sell)
		prob = get_price_prob(higher_than, my_position.price, 2);
	else
		prob = get_price_prob(lower_than, my_position.price, 2);
	return prob <= 0.32;
}

static enum action_type price_comparer(double latest, int idx)
{
	enum action_type action = action_observe;
	struct timely_indicator *ind = indicators.timely + idx;
	double profit, prob, mindiff;
	double ret = get_return();
	const double open_prob = 0.65, close_prob = 0.65;

	enum {
		lower_than = -1,
		higher_than = 1
	};

	if (!ind->available)
		return action_observe;

	mindiff = (policy.min_open_profit + fee) / my_position.quantity;
	switch (my_position.mode << 1 | my_position.status) {
	case 0b00:
		prob = get_price_prob(
			higher_than, latest + mindiff, idx);
		if (prob >= open_prob && ret >= 0.0005)
			action = action_buy;
		break;
	case 0b10:
		prob = get_price_prob(
			lower_than, latest - mindiff, idx);
		if (prob >= open_prob && ret <= -0.0005)
			action = action_sell;
		break;
	case 0b01:
		prob = get_price_prob(higher_than, latest, idx);
		profit = cal_profit(latest);
		if (ret > 0.0005 || (prob > close_prob && ret > -0.0003))
			action = action_observe;
		else if (profit > policy.min_close_profit)
			action = action_sell;
		break;
	case 0b11:
		prob = get_price_prob(lower_than, latest, idx);
		profit = cal_profit(latest);
		if (ret < -0.0005 || (prob > close_prob && ret < 0.0003))
			action = action_observe;
		else if (profit > policy.min_close_profit)
			action = action_buy;
		break;
	}
	printf("prob%d=%e, ", idx, prob);
	return action;
}

void analyzer_cleanup(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(indicators.timely); i++) {
		g_list_free_full(
			indicators.timely[i].probdist,
			free_price_interval);
	}
}

void analyze()
{
	enum action_type action = action_observe;
	struct trade *trade = (struct trade *)g_list_last(market.trades)->data;
	double latest = trade->price;
	int i;

	update_indicators();
	dump_indicators();
	for (i = 0; i < ARRAY_SIZE(indicators.timely) &&
		     action == action_observe; i++) {
		action = price_comparer(latest, i);
	}

	switch (my_position.status) {
	case incomplete: {
		double profit = cal_profit(latest);
		int trapped;
		printf("\tprof=%.2f, ", profit);
		if (profit > 0 && action != action_observe)
			break;
		/* early-morning trading */
		if (my_position.status == incomplete &&
		    !indicators.timely[0].available &&
		    profit > policy.min_close_profit) {
			action = my_position.mode == buy_and_sell ?
				action_sell : action_buy;
			break;
		}
		trapped = is_trapped();
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
	if (action != action_observe) {
		execute(action);
	} else {
		printf("\n");
	}
	discard_old_records(g_list_length(market.trades) - MAX_TRADES_COUNT);
	fflush(stdout);
}

