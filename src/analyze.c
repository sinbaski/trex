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
Engine *mateng = NULL;
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
				strcpy(my_position.time, get_timestring());
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
	struct trade *trade = (struct trade *)
		g_list_last(market.trades)->data;
	char msg[256], matbuf[256], buffer[512];
	int l;

#if CURFEW_AFT_5
	if (my_flags.allow_new_entry && strcmp(trade->time, "16:30:00") > 0)
		my_flags.allow_new_entry = 0;
#endif

	if (! my_flags.do_trade || (my_position.status == complete &&
				   ! my_flags.allow_new_entry))
		return;
	if (!mateng) {
		mateng = engOpen("matlab");
		if (!mateng) {
			fprintf(stderr, "\nCan't start MATLAB engine\n");
			return;
		} else {
			memset(matbuf, 0, sizeof(matbuf));
			engOutputBuffer(mateng, matbuf, sizeof(matbuf));
			engEvalString(mateng, "addpath('matlab_scripts');");
			/* engSetVisible(mateng, 0); */
		}
	}
	sprintf(buffer, "mysql%1$s = database('avanza', "
		"'sinbaski', 'q1w2e3r4', "
		"'com.mysql.jdbc.Driver', "
		"'jdbc:mysql://localhost:3306/avanza');",
		stockinfo.dataid);
	engEvalString(mateng, buffer);

	sprintf(buffer, "myposition%1$s = {%2$d, %3$d, %4$f, %5$ld, \'%6$s\'}",
		stockinfo.dataid, my_position.mode, my_position.status,
		my_position.price, my_position.quantity, my_position.time);
	engEvalString(mateng, buffer);

	sprintf(buffer,
		"[action%1$s, retcode%1$s, msg%1$s] = "
		"analyze(myposition%1$s, "
		"{'%2$s', '%3$s', '%4$s', mysql%1$s});",
		stockinfo.dataid, todays_date, trade->time, stockinfo.tbl_name);
	engEvalString(mateng, buffer);
	
	sprintf(buffer, "[action%1$s retcode%1$s]", stockinfo.dataid);
	engEvalString(mateng, buffer);
	sscanf(matbuf, ">> \nans = \n\n    %d %d", (int *)&action, &l);

	sprintf(buffer, "msg%1$s", stockinfo.dataid);
	engEvalString(mateng, buffer);
	get_mat_string(matbuf, msg);

	printf("%s; %s; ", msg, action_strings[action]);
	/* mxFree(msg); */

	if (action != action_none) {
		if ((status = execute(action)) == order_executed)
			printf("%s.", "executed");
		else
			printf("%s.", status == order_killed ?
			       "killed" : "failed");
	}
	printf("\n");
	fflush(stdout);
}

void analyzer_cleanup(void)
{
	char stmt[64];
	if (mateng == NULL)
		return;
	sprintf(stmt, "close(mysql%s);", stockinfo.dataid);
	engEvalString(mateng, stmt);
	engClose(mateng);
}
