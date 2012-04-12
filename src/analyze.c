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

/* #define get_mat_variable(eng, name, var)			\ */
/* 	({							\ */
/* 		char buf[32];					\ */
/* 		sprintf(buf, "%s%s", #name, orderbookId);	\ */
/* 		engEvalString(eng, buf */
/* 		var = *(typeof(var) *)func(mxa);		\ */
/* 		mxDestroyArray(mxa);				\ */
/* 	}) */

static char *get_mat_string(const char *matbuf, char *str)
{
	const char *cp;
	char *p;
	for (cp = matbuf; *cp != '='; cp++);
	cp++;
	while (*cp == ' ' || *cp == '\n') cp++;
	strcpy(str, cp);
	p = str + strlen(str) - 1;
	while (*p == '\n' || *p == ' ') *p-- = '\0';
	return str;
}

void analyze(void)
{
	enum action_type action = action_none;
	enum order_status status;
	static Engine *mateng = NULL;
	struct trade *trade = (struct trade *)
		g_list_last(market.trades)->data;
	char msg[256], matbuf[256], dataspec[256], buffer[512];
	static unsigned int flags = 0;
	static int model_fits = 0;
	int allow_new_positions;
	int l;

#if CURFEW_AFT_5
	allow_new_positions = strcmp(trade->time, "16:30:00") < 0;
#else
	allow_new_positions = 1;
#endif

	if (!do_trade)
		return;
	if (!mateng) {
		mateng = engOpen("matlab");
		if (!mateng) {
			fprintf(stderr, "\nCan't start MATLAB engine\n");
			return;
		} else {
			memset(matbuf, 0, sizeof(matbuf));
			engOutputBuffer(mateng, matbuf, sizeof(matbuf));
			/* engSetVisible(mateng, 0); */
		}
	}
	sprintf(dataspec, "from %1$s where tid "
		"<= \"%2$s %3$s\"", get_tbl_name(),
		todays_date, trade->time);
	switch (flags) {
	case 0x00:
		sprintf(buffer, "mysql%1$s = database('avanza', "
			"'sinbaski', 'q1w2e3r4', "
			"'com.mysql.jdbc.Driver', "
			"'jdbc:mysql://localhost:3306/avanza');",
			orderbookId);
		engEvalString(mateng, buffer);

		sprintf(buffer, "spec%1$s = garchset("
			"'Distribution', 'T', 'Display', 'off','VarianceModel',"
			" 'GJR', 'P', 1, 'Q', 1, 'R', 1, 'M', 1);",
			orderbookId);
		engEvalString(mateng, buffer);

	case 0x01: /* fall through! */
		sprintf(buffer, "data%1$s = fetch(mysql%1$s, "
			"'select count(*) %2$s order by tid desc "
			"limit %3$d');data%1$s{1, 1}\n",
			orderbookId, dataspec, DATA_SIZE);
		engEvalString(mateng, buffer);
		sscanf(matbuf, ">> \nans = \n\n    %d", &l);
		if (l < DATA_SIZE) {
			flags = 0x01;
			return;
		}
		flags = 0x02;
		sprintf(buffer, "x%1$s = fetch(mysql%1$s,"
			"'select price, volume %2$s order by tid desc "
			"limit %3$d');\n"
			"data%1$s = flipud(x%1$s);",
			orderbookId, dataspec, DATA_SIZE);
		engEvalString(mateng, buffer);

		sprintf(buffer,
			"price%1$s = cell2mat(data%1$s(:, 1));\n"
			"volume%1$s = cell2mat(data%1$s(:, 2));\n"
			"clear data%1$s;",
			orderbookId);
		engEvalString(mateng, buffer);
		break;

	case 0x02: {
		const GList *node =
			g_list_nth(market.trades, g_list_length(market.trades)
				   - market.new_trades);
		int i;
		sprintf(buffer,
			"price%1$s = circshift(price%1$s, %2$d);"
			"volume%1$s = circshift(volume%1$s, %2$d);",
			orderbookId, -market.new_trades);
		engEvalString(mateng, buffer);
		for (i = 0; i < market.new_trades; i++, node = node->next) {
			const struct trade *trade =
				(struct trade *)node->data;
			sprintf(buffer,
				"price%1$s(%2$d) = %3$f;\n"
				"volume%1$s(%2$d) = %4$ld;",
				orderbookId,
				DATA_SIZE - market.new_trades + i + 1,
				trade->price, trade->quantity);
			engEvalString(mateng, buffer);			
		}
		break;
	}
	default:
		break;
	}

	sprintf(buffer, "myposition%1$s = {%2$d, %3$d, %4$f, %5$ld}",
		orderbookId, my_position.mode, my_position.status,
		my_position.price, my_position.quantity);
	engEvalString(mateng, buffer);

	sprintf(buffer,
		"[spec%1$s, action%1$s, retcode%1$s, msg%1$s] = "
		"analyze(myposition%1$s, price%1$s, volume%1$s, "
		"spec%1$s, %2$d, {'%3$s', '%4$s', '%5$s'}, mysql%1$s);",
		orderbookId, model_fits, todays_date, trade->time,
		get_tbl_name());
	engEvalString(mateng, buffer);
	
	sprintf(buffer, "[action%1$s retcode%1$s]", orderbookId);
	engEvalString(mateng, buffer);
	sscanf(matbuf, ">> \nans = \n\n    %d %d", (int *)&action, &l);

	sprintf(buffer, "msg%1$s", orderbookId);
	engEvalString(mateng, buffer);
	get_mat_string(matbuf, msg);

	model_fits = l >= 0;

	printf("%s; %s; ", msg, action_strings[action]);
	/* mxFree(msg); */

	if (action != action_none &&
	    !(my_position.status == complete && !allow_new_positions)) {
		if ((status = execute(action)) == order_executed)
			printf("%s.", "executed");
		else
			printf("%s.", status == order_killed ?
			       "killed" : "failed");
	}
	printf("\n");
	fflush(stdout);
}

