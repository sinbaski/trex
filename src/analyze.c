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

#define get_mat_variable(eng, name)				\
	({							\
		char buf[100];					\
		sprintf(buf, "%s%s", #name, orderbookId);	\
		engGetVariable(eng, buf);			\
	})

void analyze(void)
{
	enum action_type action = action_none;
	enum order_status status;
	static Engine *mateng = NULL;
	struct trade *trade = (struct trade *)
		g_list_last(market.trades)->data;
	static int count = 0;
	static int model_fits = 0;
	int allow_new_positions;
	char buffer[500];
	char *msg;
	FILE *datafile;
	mxArray *mxa;
	int l;
#if CURFEW_AFT_5
	allow_new_positions = strcmp(trade->time, "16:30:00") < 0;
#else
	allow_new_positions = 1;
#endif

	if (strcmp(calibration, "no") == 0)
		return;
	if (!mateng) {
		mateng = engOpen(
			"matlab");
		if (!mateng) {
			fprintf(stderr, "\nCan't start MATLAB engine\n");
			return;
		} else
			engSetVisible(mateng, 0);
	}
	/* fp = fopen(get_filename("positions", ".pos"), "w"); */
	/* fprintf(fp, "%d\t%d\t%f\t%ld\n", */
	/* 	my_position.mode, my_position.status, */
	/* 	my_position.price, my_position.quantity */
	/* 	); */
	/* fclose(fp); */

	if (!count) {
		sprintf(buffer, "spec%1$s = garchset("
			"'Distribution', 'T', 'Display', 'off','VarianceModel',"
			" 'GJR', 'P', 1, 'Q', 1, 'R', 1, 'M', 1);",
			orderbookId);
		engEvalString(mateng, buffer);

		sprintf(buffer, "[spec%1$s, action%1$s, retcode%1$s, msg%1$s] = analyze("
			"'%1$s', %2$d, %3$d, %4$f, %5$ld, "
			"spec%1$s, 0, '%6$s', 0);",
			orderbookId, my_position.mode,
			my_position.status, my_position.price,
			my_position.quantity, calibration);
		engEvalString(mateng, buffer);

		mxa = get_mat_variable(mateng, retcode);
		l = *(int8_t *)mxGetData(mxa);
		mxDestroyArray(mxa);
		model_fits = l >= 0;

		count++;
		return;

	}
	datafile = fopen(get_filename("records", ".dat"), "r");
	l = fnum_of_line(datafile);
	fclose(datafile);
	if (l < MIN_ANALYSIS_SIZE) {
		sprintf(buffer, "[spec%1$s, action%1$s, retcode%1$s, msg%1$s] "
			"= analyze('%1$s', %2$d, %3$d, %4$f, %5$ld, "
			"spec%1$s, %6$d, '%7$s', 0);", orderbookId,
			my_position.mode, my_position.status,
			my_position.price, my_position.quantity, model_fits,
			calibration);
		engEvalString(mateng, buffer);

		mxa = get_mat_variable(mateng, spec);
		mxDestroyArray(mxa);

		mxa = get_mat_variable(mateng, action);
		mxDestroyArray(mxa);

		mxa = get_mat_variable(mateng, retcode);
		l = *(int8_t *)mxGetData(mxa);
		mxDestroyArray(mxa);
		model_fits = l >= 0;

		mxa = get_mat_variable(mateng, msg);
		mxDestroyArray(mxa);

		return;
	}

	sprintf(buffer, "[spec%1$s, action%1$s, retcode%1$s, msg%1$s] = analyze("
		"'%1$s', %2$d, %3$d, %4$f, %5$ld, "
		"spec%1$s, %6$d, '%7$s', 1);", orderbookId, my_position.mode,
		my_position.status, my_position.price, my_position.quantity,
		model_fits, calibration);
	engEvalString(mateng, buffer);

	mxa = get_mat_variable(mateng, action);
	action = *(int8_t *)mxGetData(mxa);
	mxDestroyArray(mxa);

	mxa = get_mat_variable(mateng, msg);
	msg = mxArrayToString(mxa);
	mxDestroyArray(mxa);

	mxa = get_mat_variable(mateng, retcode);
	l = *(int8_t *)mxGetData(mxa);
	mxDestroyArray(mxa);
	model_fits = l >= 0;

	printf("%s; %s; ", msg, action_strings[action]);
	mxFree(msg);

	if (l) goto end;
	if (action != action_none &&
	    !(my_position.status == complete && !allow_new_positions)) {
		if ((status = execute(action)) == order_executed)
			printf("%s.", "executed");
		else
			printf("%s.", status == order_killed ?
			       "killed" : "failed");
	}
end:
	printf("\n");
	fflush(stdout);
}

