#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <errno.h>
#include <curl/curl.h>
#include <glib.h>
#include <glib/gprintf.h>
#include "analyze.h"

#define MIN_NEW_TRADES 10
#define TRANSFER_TIMEOUT 60 * 5 /* 5 minutes */
#define WORKDIR "/home/xxie/work/avanza/data_extract/intraday"
#define DATA_UPDATE_INTERVAL 36

enum status {
	registering,
	collecting,
	finished
};

enum order_status {
	order_executed = 0,
	order_in_market,
	order_waiting,
	order_killed
};

struct connection {
	CURL *handle;
	char errbuf[CURL_ERROR_SIZE];
} conn = {
	.handle = NULL,
};

struct stock_info {
	const char *name;
	const char *dataid;
	const char *orderid;
} stockinfo[] = {
	{
		"Boliden",
		"183828",
		"5564"
	},
};

char orderbookId[20];
volatile enum status my_status;

#if DAEMONIZE
static void daemonize(void)
{
	pid_t pid, sid;
	/* already a daemon */
	if ( getppid() == 1 ) return;

	/* Fork off the parent process */
	pid = fork();
	if (pid < 0) {
		exit(-1);
	}
	/* If we got a good PID, then we can exit the parent process. */
	if (pid > 0) {
		exit(0);
	}

	/* At this point we are executing as the child process */

	/* Change the file mode mask */
	umask(0);

	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0) {
		exit(-1);
	}

	/* Change the current working directory.  This prevents the current
	   directory from being locked; hence not being able to remove it. */
	if ((chdir(WORKDIR)) < 0) {
		exit(-1);
	}
}
#endif

static void declare(const char *str)
{
	FILE *fp = fopen("./beep", "a");
	fprintf(fp, "%s\n", str);
	fclose(fp);

}

static const struct stock_info *get_stock_info(const char *id)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(stockinfo); i++) {
		if (strcmp(stockinfo[i].dataid, id) == 0) {
			return stockinfo + i;
		}
	}
	return NULL;
}

static void free_trade (void *p)
{
	g_slice_free(struct trade, p);
}

static void log_data(const GList *node)
{
	FILE *datafile = fopen(get_filename("records", ".dat"), "a");
	if (!datafile) {
		fprintf(stderr, "%s: Failed to open datafile. Error %d.\n",
			__func__, errno);
		return;
	}
	while (node) {
		const struct trade *trade =
			(struct trade *)node->data;
		fprintf(datafile, "%6s\t%8s\t%7.2lf\t%7ld\n",
			trade->market, trade->time,
			trade->price, trade->quantity);
		node = node->next;
	}
	fclose(datafile);
}

static void refresh_conn(void)
{
	curl_easy_reset(conn.handle);
	curl_easy_setopt(conn.handle, CURLOPT_ERRORBUFFER, conn.errbuf);
	curl_easy_setopt(conn.handle, CURLOPT_TIMEOUT, TRANSFER_TIMEOUT);
}

#if REAL_TRADE
static void extract_header_field(const char *field, GList **values, FILE *fp)
{
	GRegex *regex;
	GString *gstr = g_string_sized_new(128);
	char buffer[1000];
	GError *error = NULL;

	g_string_printf(gstr, "%s: *(.+)$", field);
	regex = g_regex_new(gstr->str, 0, 0, &error);
	while (fgets(buffer, sizeof(buffer), fp)) {
		GMatchInfo *info;
		gchar *str;

		g_regex_match(regex, buffer, 0, &info);
		if (!g_match_info_matches(info))
			continue;
		str = g_strdup(g_match_info_fetch(info, 1));
		*values = g_list_prepend(*values, str);
	}
	g_regex_unref(regex);
	g_string_free(gstr, TRUE);
}

static enum order_status get_order_status(FILE *fp)
{
	assert(fp != NULL);
	return order_executed;
}
#endif

static int extract_data(const char *buffer, struct trade *trade,
			const GRegex *regex)
{
	GMatchInfo *match_info;
	int ret;
	g_regex_match(regex, buffer, 0, &match_info);
	if (g_match_info_matches(match_info)) {
		gchar *str;

		str = g_match_info_fetch(match_info, 1);
		strcpy(trade->market, str);
		g_free(str);

		str = g_match_info_fetch(match_info, 2);
		strcpy(trade->time, str);
		g_free(str);
		
		str = g_match_info_fetch(match_info, 3);
		str[strlen(str) - 3] = '.';
		sscanf(str, "%lf", &trade->price);
		g_free(str);

		str = g_match_info_fetch(match_info, 4);
		sscanf(str, "%ld", &trade->quantity);
		g_free(str);

		ret = 0;
	} else
		ret = -1;
	g_match_info_free(match_info);
	return ret;
}

void discard_old_records(int size)
{
	int i;
	for (i = 0; i < size; i++) {
		g_slice_free(struct trade, market.trades->data);
		market.trades = g_list_delete_link(market.trades,
						   market.trades);
	}
	market.trades_count -= size;
	fflush(stderr);
}

static void refine_data(FILE *fp, const GRegex *regex)
{
	char buffer[1000];
	enum {
		before, between, within, after
	} position = before;
	GList *last;
	int new_trades = market.trades_count;
	while (fgets(buffer, sizeof(buffer), fp) &&
		position != after) {
		int len = strlen(buffer);

		switch (position) {
		case before:
			if (g_strstr_len(buffer, len, "Avslut "))
				position = between;
			break;
		case between:
			if (g_strstr_len(buffer, sizeof(buffer),
					 "Antal</TD></TR>"))
				position = within;
			break;
		case within: {
			struct trade trade;
			if (g_str_has_prefix(buffer, "</TABLE>"))
				goto end;
			if (extract_data(buffer, &trade, regex)) {
				fprintf(stderr, "%s: Invalid record:\n", __func__);
				fprintf(stderr, "%s\n", buffer);
				continue;
			}
			if (!market.trades) {
				struct trade *p =
					g_slice_dup(struct trade, &trade);
				if (!p) {
					fprintf(stderr, "%s: No memory.\n",
						__func__);
					g_atomic_int_set(&my_status, finished);
					return;
				}
				/* fprintf(stderr, "%s: allocated %p.\n", __func__, p); */
				market.trades = g_list_append(
					market.trades, p);
				market.newest = market.trades;
				market.trades_count = 1;
			} else if (trade_equal(&trade, (struct trade *)
					       market.newest->data)) {
				goto end;
			} else {
				struct trade *p =
					g_slice_dup(struct trade, &trade);
				if (!p) {
					fprintf(stderr, "%s: No memory.\n",
						__func__);
					fflush(stderr);
					g_atomic_int_set(&my_status, finished);
					return;
				}
				/* fprintf(stderr, "%s: allocated %p.\n", __func__, p); */
				market.trades = g_list_insert_before(
					market.trades, market.newest->next, p);
				market.trades_count++;
			}
			break;
		}
		default:
			;
		}
	}
end:
	new_trades = market.trades_count - new_trades;
	last = g_list_last(market.trades);
	/* No data added */
	if (last == market.newest)
		return;
	else if (market.newest == market.trades) {
		market.trades = g_list_remove_link(market.trades,
						   market.newest);
		last->next = market.newest;
		market.newest->prev = last;
		market.newest->next = NULL;
		market.earliest_updated = market.trades;
		log_data(market.trades);
		if (market.trades_count >= MIN_ANALYSIS_SIZE)
			analyze();
	} else {
		log_data(market.newest->next);
		market.earliest_updated = market.newest->next;
		market.newest = last;
		if (!indicators_initialized() &&
		    market.trades_count < MIN_ANALYSIS_SIZE) {
			return;
		}
		if (indicators_initialized() && new_trades < MIN_NEW_TRADES)
			return;
		analyze();
	}
}

static int login(void)
{
	int ret = 0;
#if !USE_FAKE_SOURCE
	int code;
	const char *loginfo = "username=sinbaski&password=2Oceans%3F";
	const char *url ="https://www.avanza.se/aza/login/login.jsp";
	FILE *fp;
	refresh_conn();
	curl_easy_setopt(conn.handle, CURLOPT_POSTFIELDS, loginfo);
	curl_easy_setopt(conn.handle, CURLOPT_POSTFIELDSIZE, strlen(loginfo));
	curl_easy_setopt(conn.handle, CURLOPT_URL, url);
	g_atomic_int_set(&my_status, registering);

	fp = fopen("./login.txt", "w");
	curl_easy_setopt(conn.handle, CURLOPT_HEADERDATA, fp);
	if ((code = curl_easy_perform(conn.handle))) {
		fprintf(stderr, "[%s] %s: curl_easy_perform() failed. "
			"Error %d: %s\n", get_timestring(), __func__,
			code, conn.errbuf);
		sleep(100 + rand() % 140);
		ret = code;
		goto end;
	}
	fclose(fp);
end:
	fflush(stderr);
#else
	g_atomic_int_set(&my_status, registering);	
#endif
	return ret;

}

static void prepare_connection(void)
{
	CURLcode code;
	if (conn.handle) {
		curl_easy_cleanup(conn.handle);
		curl_global_cleanup();
		conn.handle = NULL;
	}
	while (!conn.handle) {
		if ((code = curl_global_init(CURL_GLOBAL_ALL))) {
			fprintf(stderr, "curl_global_init failed "
				"with %u\n", code);
			goto end0;
		}
		if (!(conn.handle = curl_easy_init())) {
			fprintf(stderr, "curl_easy_init() failed.\n");
			goto end1;
		}
		/* "" enables the libcurl cookie engine */
		curl_easy_setopt(conn.handle, CURLOPT_COOKIEFILE, "");
		if ((code = login())) {
			goto end2;
		}
		refresh_conn();
		break;

	end2:
		curl_easy_cleanup(conn.handle);
		conn.handle = NULL;
		fflush(stderr);
	end1:
		curl_global_cleanup();
	end0:
		continue;
	}
	g_atomic_int_set(&my_status, collecting);
}

static void collect_data(void)
{
	GRegex *regex;
	GError *error = NULL;
	GString *gstr = g_string_sized_new(128);
	int i;
	regex = g_regex_new("^<TR.*><TD.*>[^<]*</TD>"
			    "<TD.*>[^<]*</TD>"
			    "<TD.*>([A-Z]+)</TD>"
			    "<TD.*>([0-9]{2}:[0-9]{2}:[0-9]{2})</TD>"
			    "<TD.*>([0-9]+,[0-9]{2})</TD>"
			    "<TD.*>([0-9]+)</TD></TR>$",
			    0, 0, &error);
	g_string_printf(gstr, "https://www.avanza.se/aza/"
			"aktieroptioner/kurslistor/"
			"avslut.jsp?password=2Oceans?"
			"&orderbookId=%s&username=sinbaski",
			orderbookId);

	prepare_connection();
	for (i = 0; g_atomic_int_get(&my_status) == collecting; i = 1) {
		struct stat st;
		FILE *fp;
#if !USE_FAKE_SOURCE
		CURLcode code;
		time_t t1, t2;
#endif

		fp = fopen("./intraday.html", "w");
		if (i == 0)
			memset(&market, 0, sizeof(market));

#if !USE_FAKE_SOURCE
		time(&t1);
		refresh_conn();
		curl_easy_setopt(conn.handle, CURLOPT_WRITEDATA, fp);
		curl_easy_setopt(conn.handle, CURLOPT_URL, gstr->str);
		if ((code = curl_easy_perform(conn.handle))) {
			char *timestring;
			timestring = get_timestring();
			fprintf(stderr, "[%s] %s: curl_easy_perform() failed. "
				"Error %d: %s\n", timestring, __func__,
				code, conn.errbuf);
			fflush(stderr);
			sleep(100 + rand() % 140);
			prepare_connection();
			fclose(fp);
			continue;
		}
		fclose(fp);
#else
		FILE *req, *resp;
		char message[10];
		/* Request data */
		req = fopen("./req-fifo", "w");
		if (!req) {
			my_status = finished;
			continue;
		}
		fprintf(req, "get");
		fclose(req);
		/* Wait until the data is returned */
		resp = fopen("./resp-fifo", "r");
		fscanf(resp, "%s", message);
		fclose(resp);
		if (strncmp(message, "done", sizeof(message)))
			continue;
#endif
		stat("./intraday.html", &st);
		if (st.st_size) {
			fp = fopen("./intraday.html", "r");
			refine_data(fp, regex);
			fclose(fp);
		}
		fflush(stderr);
#if !USE_FAKE_SOURCE
		time(&t2);
		if (t2 - t1 < DATA_UPDATE_INTERVAL)
			sleep(DATA_UPDATE_INTERVAL - (t2 - t1));
#endif
	}
	curl_easy_cleanup(conn.handle);
	curl_global_cleanup();
	g_list_free_full(market.trades, free_trade);
	g_regex_unref(regex);
	g_string_free(gstr, TRUE);
}

int send_order(enum action_type action)
{
	const struct stock_info *info = get_stock_info(orderbookId);
	struct trade *trade = (struct trade *)market.newest->data;
	double latest = trade->price;
	GString *gstr;
	int ret;
#if REAL_TRADE

	GList *list = NULL;
	FILE *fp, *fp1;
	enum order_status order_status = order_waiting;
	CURLcode err;

	assert(info != NULL);
	refresh_conn();
	fp = fopen("./OrderResponseHeader.txt", "w");
	curl_easy_setopt(conn.handle, CURLOPT_HEADERDATA, fp);
	gstr = g_string_sized_new(512);
	g_string_printf(
		gstr,
		"advanced=true&account=7781011&orderbookId=%s&market=INET&"
		"volume=%ld.0&price=%.1f&openVolume=0.0&validDate=%s&"
		"condition=AM&orderType=%s&intendedOrderType=buy&"
		"toggleClosings=false&toggleOrderDepth=false&"
		"toggleAdvanced=false&"
		"searchString=%s&transitionId=%s&commit=true&contractSize=1.0&"
		"market=INET&currency=SEK&currencyRate=1.0&countryCode=SE&"
		"orderType=sell&priceMultiplier=1.0&popped=false",
		info->orderid, my_position.quantity, latest, get_datestring(),
		action == action_buy ? "buy" : "sell", info->name,
		action == action_buy ? "21" : "31"
		);
	curl_easy_setopt(conn.handle, CURLOPT_POSTFIELDS, gstr->str);
	curl_easy_setopt(conn.handle, CURLOPT_POSTFIELDSIZE, gstr->len);
	g_string_free(gstr, TRUE);
	curl_easy_setopt(
		conn.handle, CURLOPT_URL,
		"https://www.avanza.se/aza/order/aktie/kopsalj.jsp");
	if ((err = curl_easy_perform(conn.handle))) {
		fprintf(stderr, "[%s] %s: curl_easy_perform() failed. "
			"Error %d: %s\n", get_timestring(), __func__,
			err, conn.errbuf);
		return err;
	}
	fclose(fp);
	
	/* confirm that the order has been executed */
	fp = fopen("./OrderResponseHeader.txt", "r");
	extract_header_field("Location", &list, fp);
	fclose(fp);
	if (!list) {
		ret = -1;
		goto end;
	}
	refresh_conn();
	curl_easy_setopt(conn.handle, CURLOPT_URL, (char *)list->data);
	while (order_status != order_executed &&
	       order_status != order_killed) {
		fp = fopen("./OrderResponse.html", "w");
		fp1 = fopen("./OrderResponse.txt", "w");
		curl_easy_setopt(conn.handle, CURLOPT_WRITEDATA, fp);
		curl_easy_setopt(conn.handle, CURLOPT_HEADERDATA, fp1);
		if ((err = curl_easy_perform(conn.handle))) {
			fprintf(stderr, "[%s] %s: curl_easy_perform() failed. "
				"Error %d: %s\n", get_timestring(), __func__,
				err, conn.errbuf);
			ret = err;
			order_status = order_killed;
			break;
		}
		order_status = get_order_status(fp);
		fclose(fp1);
		fclose(fp);
		if (order_status != order_executed &&
		    order_status != order_killed)
			sleep(15);
	}
	ret = order_status == order_executed ? 0 : 1;
end:
	g_list_free_full(list, g_free);
#else
	ret = 0;
#endif
	gstr = g_string_sized_new(128);
	g_string_printf(
		gstr,
		"Ladies and gentlemen, may I have your attention? "
		"I %s to %s %s at %.1f kronor.\n",
		ret == 0 ? "Succeeded" : "Failed",
		action == action_buy ? "buy" : "sell",
		info->name, latest);
	declare(gstr->str);
	g_string_free(gstr, TRUE);
	return ret;
}

static void signal_handler(int sig, siginfo_t * siginfo, void *context)
{
	char *timestring = get_timestring();
	fprintf(stderr, "[%s] signal = %d, si_code = %d.\n", timestring, sig,
		siginfo->si_code);
	if (siginfo->si_code == SI_USER || siginfo->si_code == SI_QUEUE) {
		fprintf(stderr, "si_pid = %d, si_uid = %d.\n",
			siginfo->si_pid, siginfo->si_uid);
	}
	switch (sig) {
	case SIGTERM:
		g_atomic_int_set(&my_status, finished);
		save_indicators();
		break;
	case SIGPIPE:
		break;
	default:;
	}
	fflush(stderr);
}

int main(int argc, const char *argv[])
{
	struct trade_position po;
	struct sigaction act = {
		.sa_sigaction = signal_handler,
		.sa_flags = SA_SIGINFO
	};
	int cared_signals[] = {
		SIGTERM, SIGPIPE
	};
	struct rlimit rlim = {
		RLIM_INFINITY, RLIM_INFINITY
	};
	int i;
	if (argc < 7) {
		fprintf(stderr, "Usage: \n"
			"%s orderbookId mode status price quantity\n", argv[0]);
		return 0;
	}
	strcpy(orderbookId, argv[1]);
#if DAEMONIZE
	daemonize();
#endif
	freopen( "/dev/null", "r", stdin);
	freopen(get_filename("transactions", ".txt"), "a", stdout);
	freopen(get_filename("logs", ".log"), "a", stderr);

	setrlimit(RLIMIT_CORE, &rlim);

	memset(&po, 0, sizeof(po));
	sscanf(argv[2], "%d", (int *)&po.mode);
	sscanf(argv[3], "%d", (int *)&po.status);
	sscanf(argv[4], "%lf", &po.price);
	po.enter_time = parse_time(argv[5]);
	sscanf(argv[6], "%ld", &po.quantity);
	set_position(&po);

	for (i = 0; i < sizeof(cared_signals)/sizeof(cared_signals[0]); i++)
		sigaction(cared_signals[i], &act, NULL);
	restore_indicators();
	srand(time(NULL));
	collect_data();
	return 0;
}
