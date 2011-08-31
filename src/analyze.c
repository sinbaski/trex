#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include "analyze.h"

const double fee = 198;
struct market market;
struct trade_position my_position;

const char *action_strings[number_of_action_types] = {
	"BUY", "SELL"
};

struct indicators indicators = {
	.average = 0,
	.volume = 0,
	.tolerated_loss = 0
};

struct conclusion {
	enum action_type action;
	const char *analyzer;
};

typedef enum action_type (*analyzer)(struct conclusion *);

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

static void update_indicators(void)
{
	GList *p;
	time_t now, diff;
	struct tm *timep;

	/* average & volume */
	for (p = market.earliest_updated; p; p = p->next) {
		struct trade *trade = (struct trade*)p->data;
		indicators.volume += trade->quantity;
		indicators.average +=
			(trade->price * trade->quantity)/indicators.volume -
			indicators.average * trade->quantity / indicators.volume;
	}
	time(&now);
	timep = localtime(&now);
	indicators.allow_new_positions =
		(timep->tm_hour < 17) ? 1 : 0;
	if (my_position.status == complete || timep->tm_hour < 15) {
		indicators.margin = 0.00625;
		indicators.tolerated_loss = 0;
		return;
	}
	diff = now - my_position.enter_time;
	if (diff >= 2 * SEC_AN_HOUR && diff < 3 * SEC_AN_HOUR) {
		indicators.margin = 0.0025;
	} else if (diff >= 3 * SEC_AN_HOUR && diff < 4 * SEC_AN_HOUR) {
		indicators.margin = 0;
		indicators.tolerated_loss = 200;
	} else if (diff >= 4 * SEC_AN_HOUR) {
		indicators.margin = -0.0025;
		indicators.tolerated_loss = 400;
	}
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

static void transact(const struct conclusion *conclusion)
{
	struct trade *trade = (struct trade *)market.newest->data;
	double price = trade->price;
	double profit;
	const char *trade_time = trade->time;

	printf("%s: %s %s at %f: ", conclusion->analyzer, trade_time,
	       action_strings[conclusion->action], price);

	switch (my_position.mode << 2 | my_position.status << 1 |
		conclusion->action) {
	case 0b000: /*buy-and-sell, complete, buy */
	case 0b101: /*sell-and-buy, complete, sell */
		if (!indicators.allow_new_positions) {
			printf("New positions disallowed.\n");
			break;
		}
		printf("Executed.\n");
		my_position.status = incomplete;
		my_position.price = price;
		time(&my_position.enter_time);
		break;
	case 0b001: /*buy-and-sell, complete, sell */
	case 0b010: /*buy-and-sell, incomplete, buy */
	case 0b100: /*sell-and-buy, complete, buy */
	case 0b111: /*sell-and-buy, incomplete, sell */
		printf("Rejected. Impossible now.\n");
		break;
	case 0b011: /*buy-and-sell, incomplete, sell */
	case 0b110: /*sell-and-buy, incomplete, buy */
		profit = calculate_profit(price);
		if (profit > 0 || -profit < indicators.tolerated_loss) {
			printf("Executed.\n");
			my_position.status = complete;
		} else {
			printf("Rejected. Too much loss.\n");
		}
		break;
	}
	printf("\n");
	fflush(stdout);
}

int trade_equal(const struct trade *t1, const struct trade *t2)
{
	return 	strcmp(t1->market, t2->market) == 0 &&
		strcmp(t1->time, t2->time) == 0 &&
		t1->price - t2->price > -0.01 &&
		t1->price - t2->price < 0.01 &&
		t1->quantity == t2->quantity;
}

static double average_price(int index, int size)
{
	GList *p = g_list_nth(market.trades, index);
	int i;
	double avg;
	long total;
		
	for (i = 0, avg = 0, total = 0; i < size; i++) {
		struct trade *trade = (struct trade *)p->data;
		total += trade->quantity;
		avg += (trade->price * trade->quantity)/total -
			avg * trade->quantity / total;
	}
	return avg;
}

#if 0
static enum action_type clear_trend(struct conclusion *conclusion)
{
#define GRP_NUM 3
	int group_size = 10;
	int i;
	int flag;
	double avg[GRP_NUM];
	double limit = 0.025;
	struct trade *trade = (struct trade *)market.newest->data;
	int offset = market.trades_count - GRP_NUM * group_size;

	conclusion->analyzer = __func__;
	for (i = 0; i < GRP_NUM; i++)
		avg[i] = average_price(offset + i * group_size, group_size);
	for (i = 0;
	     i < GRP_NUM - 1 && (flag = price_lower(avg[i], avg[i + 1]));
	     i++);
	if (flag && trade->price < indicators.average * (1 + limit))
		return conclusion->action = action_buy;
	for (i = 0;
	     i < GRP_NUM - 1 && (flag = price_lower(avg[i + 1], avg[i]));
	     i++);
	if (flag && trade->price > indicators.average * (1 - limit))
		return conclusion->action = action_sell;
	/* Situation unclear */
	return action_unclear;
#undef GRP_NUM
}
#endif

static enum action_type local_moving(struct conclusion *conclusion)
{
	const int span = BATCH_SIZE;
	int offset = market.trades_count - span;
	double latest = ((struct trade *)market.newest->data)->price;
	double average = average_price(offset, span);
	const double margin = 0.003125;

	conclusion->analyzer = __func__;
	conclusion->action = action_unclear;
	if (latest < average * (1 - margin))
		conclusion->action = action_buy;
	else if (latest > average * (1 + margin))
		conclusion->action = action_sell;
	return conclusion->action;
}


static enum action_type happy_ending(struct conclusion *conclusion)
{
	double latest = ((struct trade *)market.newest->data)->price;

	conclusion->analyzer = __func__;
	conclusion->action = action_unclear;
	if (my_position.status == complete)
		return conclusion->action;
	if (my_position.mode == sell_and_buy &&
	    latest <=  my_position.price * (1 - indicators.margin)) {
		conclusion->action = action_buy;
	} else if (my_position.mode == buy_and_sell &&
		   latest >=  my_position.price * (1 + indicators.margin)) {
		conclusion->action = action_sell;
	}
	return conclusion->action;
}

static enum action_type global_moving(struct conclusion *conclusion)
{
	double latest = ((struct trade *)market.newest->data)->price;
	const double margin = 0.0075;

	conclusion->analyzer = __func__;
	if (!indicators_initialized())
		return conclusion->action = action_unclear;
	/* printf("%s: [%s %f, MA %f] latest %f. ", __func__, */
	/*        my_position.status == complete ? "complete" : "incomplete", */
	/*        my_position.price, indicators.average, */
	/*        latest); */
	if (latest < indicators.average * (1 - margin)) {
		/* printf("BUY.\n"); */
		conclusion->action = action_buy;
	} else if (latest > indicators.average * (1 + margin)) {
		/* printf("SELL.\n"); */
		conclusion->action = action_sell;
	} else {
		/* printf("unclear.\n"); */
		conclusion->action = action_unclear;
	}
	/* fflush(stdout); */
	return conclusion->action;
}

void analyze(void)
{
	struct conclusion conclusion = {
		action_unclear, __func__
	};
	analyzer analyzers[] = {
		global_moving,
		local_moving,
		happy_ending,
	};
	int i;

	update_indicators();
	if (!indicators.allow_new_positions) {
		analyzers[0] = happy_ending;
		analyzers[1] = global_moving;
		analyzers[2] = local_moving;
	}
	for (i = 0; i < sizeof(analyzers) / sizeof(analyzer) &&
		     conclusion.action == action_unclear; i++) {
		analyzers[i](&conclusion);
	}
	if (conclusion.action != action_unclear)
		transact(&conclusion);
}

