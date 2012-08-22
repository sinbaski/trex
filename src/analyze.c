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

/* struct trade_const trade_constants = { */
/* 	.dpricestr = "0.0001", */
/* 	/\* 4 digits after the decimal point *\/ */
/* 	.dpricepcr = 4 */
/* }; */

void set_position(const struct trade_position *position)
{
	my_position.mode = position->mode;
	my_position.price = position->price;
	my_position.quantity = position->quantity;
}

/* static double calculate_profit(double price) */
/* { */
/* 	const double feerate = 0.08e-2; */
/* 	double diff; */
/* 	if (my_position.mode == buy_and_sell) */
/* 		diff = price - my_position.price; */
/* 	else */
/* 		diff = my_position.price - price; */
/* 	return (diff - (price + my_position.price) * feerate) * my_position.quantity; */
/* } */
static enum order_status loop_execute(enum trade_status new_status,
				      double *cash, enum action_type action)
{
	struct trade *trade = (struct trade *)g_list_last(market.trades)->data;
	double price = trade->price;
	enum order_status status = order_failed;
	GString *strprice = make_valid_price(price);
	const char *ticksize = get_tick_size(price);
	/* const int limtime = 40, delay = 8; */
	const int delay = 8;
	int trials = 4;
	/* time_t t1, t2; */

	/* time(&t1); */
	while (status != order_executed && trials--) {
		if ((status = send_order(action, strprice->str))
		    == order_executed) {

			sscanf(strprice->str, "%lf", &my_position.price);
			my_position.status = new_status;
			strcpy(my_position.time, trade->time);
			if ((my_position.mode == 0 &&
			     new_status == incomplete) ||
			    (my_position.mode == 1 &&
			     new_status == complete))
				*cash -= price * my_position.quantity;
			else {
				*cash += price * my_position.quantity;
			}
			*cash -= 99;
		} else {
			/* time(&t2); */
			printf("%d ", trials);
			if (trials == 0) continue;
			sleep(delay);
			/* if (t2 + delay - t1 < limtime) { */
			/* 	trials = 0; */
			/* 	continue; */
			/* } */
			if (trials % 2 == 0) deincreament_price(
				strprice, ticksize,
				action == action_buy ? 1 : 0);
		}
	}
	g_string_free(strprice, TRUE);
	return status;
}

static enum order_status execute(enum action_type action)
{
	struct trade *trade = (struct trade *)g_list_last(market.trades)->data;
	double price = trade->price;
	double cash;
	enum order_status status = order_failed;
	FILE *pot;

	pot = fopen("pot.txt", "r+");
	flockfile(pot);
	fscanf(pot, "%lf", &cash);
	switch (my_position.mode << 2 | my_position.status << 1 | action) {
	case 0b000: /*buy-and-sell, complete, buy */
		if (cash < price * my_position.quantity) {
			printf(" %s: insufficient cash.\n", __func__);
			break;
		}
	case 0b101: /*sell-and-buy, complete, sell */
		status = loop_execute(incomplete, &cash, action);
		break;
	case 0b001: /*buy-and-sell, complete, sell */
	case 0b010: /*buy-and-sell, incomplete, buy */
	case 0b100: /*sell-and-buy, complete, buy */
	case 0b111: /*sell-and-buy, incomplete, sell */
		printf(" %s: Rejected.\n", __func__);
		break;
	case 0b110: /*sell-and-buy, incomplete, buy */
		if (cash < price * my_position.quantity) {
			printf(" %s: insufficient cash. ", __func__);
			break;
		}
	case 0b011: /*buy-and-sell, incomplete, sell */
		status = loop_execute(complete, &cash, action);
	}
	rewind(pot);
	fprintf(pot, "%.2lf\n", cash);
	funlockfile(pot);
	fclose(pot);
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

static Engine *start_matlab(char *matbuf, int size, const char *dataid)
{
	char buffer[256];
	if (mateng) {
		engClose(mateng);
	}
	mateng = engOpen("matlab");
	if (mateng == NULL) {
		fprintf(stderr, "\nCan't start MATLAB engine\n");
		return NULL;
	}
	memset(matbuf, 0, sizeof(matbuf));
	engOutputBuffer(mateng, matbuf, size);
	engEvalString(mateng, "addpath('matlab_scripts');");
	sprintf(buffer, "mysql%1$s = database('avanza', "
		"'sinbaski', 'q1w2e3r4', "
		"'com.mysql.jdbc.Driver', "
		"'jdbc:mysql://localhost:3306/avanza');",
		dataid);
	engEvalString(mateng, buffer);
	return mateng;
}

void analyze(void)
{
	enum action_type action = action_none;
	enum order_status status;
	struct trade *trade = (struct trade *)
		g_list_last(market.trades)->data;
	char msg[256] = {
		"You shouldn't have seen me."
	};
	char matbuf[256], buffer[512];
	int l;

	if (! my_flags.do_trade)
		return;

#if CURFEW_AFT_5
	if (my_flags.allow_new_entry && strcmp(trade->time, "16:00:00") > 0)
		my_flags.allow_new_entry = 0;
#endif

	if (my_position.status == complete && ! my_flags.allow_new_entry) {
		analyzer_cleanup();
		return;
	}
	if (!mateng) {
		start_matlab(matbuf, sizeof(matbuf), stockinfo.dataid);
		if (mateng == NULL)
			return;
	}
begin:
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
	if (sscanf(matbuf, ">> \nans = \n\n    %d %d",
		   (int *)&action, &l) < 2) {
		
		/* MATLAB is malfunctioning. Restart it */
		fprintf(stderr, "[%s] MATLAB malfunctioning.\n", trade->time);
		start_matlab(matbuf, sizeof(matbuf), stockinfo.dataid);
		if (mateng == NULL)
			return;
		goto begin;
	}

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
	mateng = NULL;
}
