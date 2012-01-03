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
#include "engine.h"

#define price2str(price, str)						\
	({								\
		sprintf(str, "%.*f", trade_constants.dpricepcr, price); \
	})

struct trade_position my_position;
const char *action_strings[number_of_action_types] = {
	"BUY", "SELL", "NONE"
};

struct trade_const trade_constants = {
	.dpricestr = "0.0001",
	/* 4 digits after the decimal point */
	.dpricepcr = 4
};

void set_position(const struct trade_position *position)
{
	my_position.mode = position->mode;
	my_position.price = position->price;
	my_position.quantity = position->quantity;
}

static enum order_status execute(enum action_type action)
{
	struct trade *trade = (struct trade *)g_list_last(market.trades)->data;
	double price = trade->price;
	enum order_status status;
	int trials = 3;
	const int limtime = 20, delay = 5;
	time_t t1, t2;
	time(&t1);

	switch (my_position.mode << 2 | my_position.status << 1 | action) {
	case 0b000: /*buy-and-sell, complete, buy */
	case 0b101: /*sell-and-buy, complete, sell */
		do {
			if ((status = send_order(action)) == order_executed) {
				my_position.status = incomplete;
				my_position.price = price;
			} else {
				time(&t2);
				if (--trials) {
					printf("%d ", trials);
					if (t2 + delay - t1 < limtime)
						sleep(delay);
					else
						trials = 0;
				}
			}
		} while (status != order_executed && trials);
		break;
	case 0b001: /*buy-and-sell, complete, sell */
	case 0b010: /*buy-and-sell, incomplete, buy */
	case 0b100: /*sell-and-buy, complete, buy */
	case 0b111: /*sell-and-buy, incomplete, sell */
		printf(" %s: Rejected.\n", __func__);
		break;
	case 0b011: /*buy-and-sell, incomplete, sell */
	case 0b110: /*sell-and-buy, incomplete, buy */
		do {
			if ((status = send_order(action)) == order_executed) {
				my_position.status = complete;
				my_position.trapped = 0;
			} else {
				time(&t2);
				if (--trials) {
					printf("%d ", trials);
					if (t2 + delay - t1 < limtime)
						sleep(delay);
					else
						trials = 0;
				}
			}
		} while (status != order_executed && trials);
	}
	return status;
}

void analyze(void)
{
	enum action_type action;
	enum order_status status;
	static Engine *mateng = NULL;
	static int count = 0;
	int allow_new_positions;
	char buffer[500];
	FILE *fp;
	int fit;
#if CURFEW_AFT_5
	allow_new_positions = strcmp(get_timestring(), "16:30:00") < 0;
#else
	allow_new_positions = 1;
#endif
	if (!mateng) {
		mateng = engOpen("matlab");
		if (!mateng) {
			fprintf(stderr, "\nCan't start MATLAB engine\n");
			return;
		} else
			engSetVisible(mateng, 0);
	}
	fp = fopen(get_filename("positions", ".pos"), "w");
	fprintf(fp, "%d\t%d\t%f\t%ld\n",
		my_position.mode, my_position.status,
		my_position.price, my_position.quantity
		);
	fclose(fp);

	sprintf(buffer, "spec%s", orderbookId);
	if (!count) {
		sprintf(buffer, "spec%s = garchset('Distribution', 'T',"
			"'Display', 'off','VarianceModel', 'GJR',"
			"'P', 1, 'Q', 1, 'R', 1, 'M', 1);",
			orderbookId);
		engEvalString(mateng, buffer);
	}

	sprintf(buffer, "spec%s = analyze('%s', spec%s, %d);",
		orderbookId, orderbookId, orderbookId, count++);
	engEvalString(mateng, buffer);

	sprintf(buffer, "rendezvous/%s.txt", orderbookId);
	fp = fopen(buffer, "r");
	fscanf(fp, "%d %d\n", &fit, (int *)&action);
	if (fit)
		fgets(buffer, sizeof(buffer), fp);
	fclose(fp);

	if (!fit) {
		printf("[%s] Model doesn't fit. Do nothing.\n",
		       get_timestring());
		goto end;
	}
	printf("%s; %s; ", buffer, action_strings[action]);
	if (action != action_none &&
	    !(my_position.status == complete && !allow_new_positions)) {
		if ((status = execute(action)) == order_executed)
			printf("%s.", "executed");
		else
			printf("%s.", status == order_killed ?
			       "killed" : "failed");
	}
	printf("\n");
end:
	fflush(stdout);
}

