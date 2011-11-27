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
#include "analyze.h"
#include "utilities.h"

#define MIN_NEW_TRADES 8
#define TRANSFER_TIMEOUT 60 * 5 /* 5 minutes */
#define WORKDIR "/home/xxie/work/avanza/data_extract/intraday"
#define COOKIE_FILE "./cookies.txt"
#define DATA_UPDATE_INTERVAL 36


enum status {
	registering,
	collecting,
	finished
};

struct connection {
	CURL *handle;
	char errbuf[CURL_ERROR_SIZE];
	unsigned int valid:1;
} conn = {
	.handle = NULL,
	.valid = 0
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

static int trade_equal(const struct trade *t1, const struct trade *t2)
{
	return 	strcmp(t1->market, t2->market) == 0 &&
		strcmp(t1->time, t2->time) == 0 &&
		t1->price - t2->price > -0.01 &&
		t1->price - t2->price < 0.01 &&
		t1->quantity == t2->quantity;
}

static int log_data(int idx)
{
	FILE *datafile;
	const GList *node = g_list_nth(market.trades, idx);
	struct trade last;
	static int check_redundancy = 1;
	int n;

	if (check_redundancy) {
		datafile = fopen(get_filename("records", ".dat"), "r");
		if (datafile == NULL)
			check_redundancy = 0;
		else {
			get_trade(datafile, fnum_of_line(datafile) - 1, &last);
			fclose(datafile);
		}
	}
	datafile = fopen(get_filename("records", ".dat"), "a");
	n = 0;
	while (node) {
		const struct trade *trade =
			(struct trade *)node->data;
		if (!check_redundancy || strcmp(trade->time, last.time) >= 0) {
			fprintf(datafile, "%6s\t%8s\t%7.2lf\t%7ld\n",
				trade->market, trade->time,
				trade->price, trade->quantity);
			check_redundancy = 0;
			n++;
		}
		node = node->next;
	}
	fclose(datafile);
	return n;
}

static void refresh_conn(void)
{
	curl_easy_reset(conn.handle);
	curl_easy_setopt(conn.handle, CURLOPT_ERRORBUFFER, conn.errbuf);
	curl_easy_setopt(conn.handle, CURLOPT_TIMEOUT, TRANSFER_TIMEOUT);
	curl_easy_setopt(conn.handle, CURLOPT_USERAGENT,
			 "Mozilla/5.0 (compatible; MSIE 9.0; "
			 "Windows NT 6.1; WOW64; Trident/5.0)");
	if (g_atomic_int_get(&my_status) != registering)
		curl_easy_setopt(conn.handle, CURLOPT_COOKIEFILE, COOKIE_FILE);
}

#if 0
static void write_out_cookies(void)
{
	FILE *fp = fopen("cookie_dump.txt", "w");
	struct curl_slist *list, *node;

	curl_easy_getinfo(conn.handle, CURLINFO_COOKIELIST, &list);
	fprintf(fp, "Known cookies:\n");
	for (node = list; node != NULL; node = node->next) {
		fprintf(fp, "%s\n", node->data);
	}
	fclose(fp);
	curl_slist_free_all(list);
}

static void extract_header_field(const char *field, GList **values, FILE *fp)
{
	GRegex *regex;
	GString *gstr = g_string_sized_new(128);
	char buffer[1024];
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
	fflush(stderr);
}

static void refine_data(FILE *fp, const GRegex *regex)
{
	char buffer[1000];
	enum {
		before, between, within, after
	} position = before;
	int gp = g_list_length(market.trades);
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
			if (strcmp(trade.time, "09:00:00") < 0)
				break;
			if (gp != 0 && trade_equal(
				    &trade, (struct trade *)
				    g_list_nth_data(market.trades,
						    gp - 1)))
				goto end;
			else {
				struct trade *p =
					g_slice_dup(struct trade, &trade);
				market.trades = g_list_insert(
					market.trades, p, gp);
				market.new_trades++;
			}
			break;
		}
		default:
			;
		}
	}
end:
	if (market.new_trades == 0 || log_data(gp) == 0)
		return;
	if (g_list_length(market.trades) >= MIN_ANALYSIS_SIZE &&
	    (market.new_trades >= MIN_NEW_TRADES || !indicators_initialized())) {
		analyze();
		market.new_trades = 0;
	}
}

size_t store_cookie(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	FILE *fp = (FILE *)userdata;
	char *str = (char *)ptr;
	if (!g_str_has_prefix(str, "Set-Cookie:"))
		return size * nmemb;
	fprintf(fp, "%s", str);
	return size * nmemb;
}

static int login(void)
{
	int ret = 0;
	int code;
	const char *loginfo = "username=sinbaski&password=2Oceans?";
	const char *url ="https://www.avanza.se/aza/login/login.jsp";
	/* char *str = curl_easy_escape(conn.handle, loginfo, strlen(loginfo)); */
	FILE *fp;
	g_atomic_int_set(&my_status, registering);

	refresh_conn();
	curl_easy_setopt(conn.handle, CURLOPT_POSTFIELDS, loginfo);
	curl_easy_setopt(conn.handle, CURLOPT_URL, url);
	fp = fopen(COOKIE_FILE, "w");
	curl_easy_setopt(conn.handle, CURLOPT_HEADERDATA, fp);
	curl_easy_setopt(conn.handle, CURLOPT_HEADERFUNCTION, store_cookie);
	if ((code = curl_easy_perform(conn.handle)) || !conn.valid) {
		fclose(fp);
		/* curl_free(str); */
		fprintf(stderr, "[%s] %s: curl_easy_perform() failed. "
			"Error %d: %s\n", get_timestring(), __func__,
			code, conn.errbuf);
		sleep(100 + rand() % 200);
		ret = code != 0 ? code : -EPIPE;
		goto end;
	}
	fclose(fp);
	/* curl_free(str); */
end:
	fflush(stderr);
	return ret;
}

static void prepare_connection(void)
{
	if (conn.handle) {
		curl_easy_cleanup(conn.handle);
		conn.handle = NULL;
	}
	while (!conn.handle) {
		conn.handle = curl_easy_init();
		conn.valid = 1;
		if (login()) {
			goto end;
		}
		/* Cookie settings are not reset */
		/* curl_easy_setopt(conn.handle, CURLOPT_COOKIEFILE, ""); */
		g_atomic_int_set(&my_status, collecting);
		break;

	end:
		curl_easy_cleanup(conn.handle);
		conn.handle = NULL;
		fflush(stderr);
	}
}

#if !USE_FAKE_SOURCE
static CURLcode perform_request(void)
{
	CURLcode code;
	char *timestring;
	code = curl_easy_perform(conn.handle);
	if (code == 0 && conn.valid)
		return code;

	conn.valid = 0;
	timestring = get_timestring();
	fprintf(stderr, "[%s] %s: curl_easy_perform() failed. "
		"Error %d: %s\n", timestring, __func__,
		code, conn.errbuf);
	fflush(stderr);
	sleep(100 + rand() % 200);
	prepare_connection();
	return code != 0 ? code : -EPIPE;
}

static double get_enter_price(const char *mark)
{
	/* "https://www.avanza.se/aza/order/aktie/kopsalj.jsp" */
	FILE *datafile = fopen(get_filename("transactions", ".txt"), "r");
	GRegex *regex;
	GError *error = NULL;
	GMatchInfo *match_info;
	char buffer[128], template[64];
	double x = -1;

	if (datafile == NULL)
		return -1;
	sprintf(template, "p=([0-9.]+).*%s[.].*$", mark);
	regex = g_regex_new(template, 0, 0, &error);
	while (fgets(buffer, sizeof(buffer), datafile)) {
		gchar *str;
		g_regex_match(regex, buffer, 0, &match_info);
		if (!g_match_info_matches(match_info))
			continue;
		str = g_match_info_fetch(match_info, 1);
		sscanf(str, "%lf", &x);
	}
	g_regex_unref(regex);
	return x;
}

/* We must have logged in in order to do this */
static long get_hld_qtt(void)
{
	FILE *fp = fopen("depa.html", "w");
	const char *url = "https://www.avanza.se/aza/depa/depa.jsp"
		"?depotnr=7781011";
	GRegex *regex;
	GError *error = NULL;
	char buffer[1024];
	const struct stock_info *info = get_stock_info(orderbookId);
	GMatchInfo *match_info;
	/* If no match is found, we have 0 shares. */
	long n = 0;
	int page_confirmed = 0;

	refresh_conn();
	curl_easy_setopt(conn.handle, CURLOPT_URL, url);
	curl_easy_setopt(conn.handle, CURLOPT_REFERER,
			 "https://www.avanza.se/aza/depa/"
			 "sammanfattning/sammanfattning.jsp");
	curl_easy_setopt(conn.handle, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(conn.handle, CURLOPT_AUTOREFERER, 1);
	curl_easy_setopt(conn.handle, CURLOPT_WRITEDATA, fp);
	if (perform_request()) {
		fclose(fp);
		return -1;
	}
	fclose(fp);

	fp = fopen("depa.html", "r");
	while (fgets(buffer, sizeof(buffer), fp)) {
		if (g_strrstr(buffer,
			   "<option value=\"7781011\" selected="
			   "\"selected\">7781011:") != NULL) {
			page_confirmed = 1;
			break;
		}
	}
	if (!page_confirmed) {
		fclose(fp);
		prepare_connection();
		return -1;
	}
	fseek(fp, 0, SEEK_SET);
	sprintf(buffer, ">%s</a></td><td +valign=\"bottom\" +"
		"class=\"neutral\"><nobr>([0-9]+)</nobr></td>",
		info->name);
	regex = g_regex_new(buffer, 0, 0, &error);
	while (fgets(buffer, sizeof(buffer), fp)) {
		gchar *str;
		g_regex_match(regex, buffer, 0, &match_info);
		if (!g_match_info_matches(match_info))
			continue;
		str = g_match_info_fetch(match_info, 1);
		sscanf(str, "%ld", &n);
		break;
	}
	fclose(fp);
	g_regex_unref(regex);
	return n;
}
#endif

static inline void update_watcher(void)
{
	char buf[20];
	FILE *fp;
	sprintf(buf, "watcher-%s", orderbookId);
	fp = fopen(buf, "w");
	fprintf(fp, "Active\n");
	fclose(fp);
}

static void collect_data(void)
{
	GRegex *regex;
	GError *error = NULL;
	GString *gstr = g_string_sized_new(128);
	int i;
	/* The number of shares that we have on the account */
#if !USE_FAKE_SOURCE
	long hld_qtt;
#endif
	regex = g_regex_new("^<TR.*><TD.*>[^<]*</TD>"
			    "<TD.*>[^<]*</TD>"
			    "<TD.*>([A-Z]+)</TD>"
			    "<TD.*>([0-9]{2}:[0-9]{2}:[0-9]{2})</TD>"
			    "<TD.*>([0-9]+,[0-9]{2})</TD>"
			    "<TD.*>([0-9]+)</TD></TR>$",
			    0, 0, &error);
	/* g_string_printf(gstr, "https://www.avanza.se/aza/" */
	/* 		"aktieroptioner/kurslistor/" */
	/* 		"avslut.jsp?password=2Oceans?" */
	/* 		"&orderbookId=%s&username=sinbaski", */
	/* 		orderbookId); */
	update_watcher();
	prepare_connection();

#if !USE_FAKE_SOURCE
	while ((hld_qtt = get_hld_qtt()) < 0);
	if ((my_position.mode == sell_and_buy &&
	     hld_qtt < my_position.quantity) ||
	    (my_position.mode == buy_and_sell &&
	     hld_qtt > my_position.quantity)) {
		double x;
		my_position.status = incomplete;
		x = get_enter_price(my_position.mode == buy_and_sell ?
				    "BUY" : "SELL");
		if (x > 0)
			my_position.price = x;
	} else
		my_position.status = complete;
#endif
	for (i = 0; g_atomic_int_get(&my_status) == collecting; i = 1) {
		struct stat st;
		FILE *fp;
#if !USE_FAKE_SOURCE
		time_t t1, t2;
#endif
		/* Report to the watcher that I am active */
		update_watcher();
		fp = fopen("./intraday.html", "w");
		if (i == 0)
			memset(&market, 0, sizeof(market));

#if !USE_FAKE_SOURCE
		time(&t1);
		refresh_conn();
		curl_easy_setopt(conn.handle, CURLOPT_WRITEDATA, fp);
		g_string_printf(gstr, "https://www.avanza.se/aza/"
				"aktieroptioner/kurslistor/"
				"avslut.jsp?&orderbookId=%s",
				orderbookId);
		curl_easy_setopt(conn.handle, CURLOPT_URL, gstr->str);
		g_string_printf(gstr, "https://www.avanza.se/aza/"
				"aktieroptioner/kurslistor/"
				"aktie.jsp?&orderbookId=%s",
				orderbookId);
		curl_easy_setopt(conn.handle, CURLOPT_REFERER, gstr->str);
		if (perform_request()) {
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
		} else {
			char *timestring;
			timestring = get_timestring();
			fprintf(stderr, "[%s] %s: Null conent returned. "
				"Reconnect\n", timestring, __func__);
			conn.valid = 0;
			prepare_connection();
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
	analyzer_cleanup();
	g_list_free_full(market.trades, free_trade);
	g_regex_unref(regex);
	g_string_free(gstr, TRUE);
}

#if REAL_TRADE
static int check_aktuella(FILE *fp, long m, long n, enum action_type action,
			  char buffer[], int size)
{
	const struct stock_info *info = get_stock_info(orderbookId);
	int ret = 0;
	fseek(fp, m, SEEK_SET);
	while (!ret && ftell(fp) < n && fgets(buffer, size, fp)) {
		if (strstr(buffer, info->name)) ret = 1;
	}
	return ret;
}

static int check_avslut(FILE *fp, long n, enum action_type action,
			char *buffer, int size)
{
	const struct stock_info *info = get_stock_info(orderbookId);
	int ret = 0;
	int done = 0;
	int s;
	fseek(fp, n, SEEK_SET);
	/* Advance to the table of interest */
	while (fgets(buffer, size, fp) &&
	       !strstr(buffer, "<tbody>"));
	for (s = 0; !done && fgets(buffer, size, fp) &&
		     !strstr(buffer, "</tbody>");) {
		int executed;
		switch (s) {
		case 0:
			if (strstr(buffer, "<tr>")) {
				s = 1;
				executed = -1;
			}
			break;
		case 1:
			if (strstr(buffer, ">K&ouml;p</td>") ||
			    strstr(buffer, ">K\366p</td>")) {
				executed = action_buy;
			} else if (strstr(buffer, ">S&auml;lj</td>") ||
				   strstr(buffer, ">S\344lj</td>")) {
				executed = action_sell;
			} else if (strstr(buffer, info->name)) {
				done = 1;
				ret = action == executed;
			} else if (strstr(buffer, "</tr>")) {
				s = 0;
			}
			break;
		default:;
		}
	}
	return ret;
}

static enum order_status get_order_status(enum action_type action)
{
	enum order_status status;
	do {
		FILE *fp = fopen("./OrderResponse.html", "r");
		long m, n;
		char buffer[1024];
		CURLcode err;
		short int k;
		for (m = 0, n = 0, k = 0;
		     n == 0 && fgets(buffer, sizeof(buffer), fp);) {
			switch(k) {
			case 0:
				if (!strstr(buffer, ">Mina aktuella order<"))
					continue;
				m = ftell(fp);
				k = 1;
				break;
			case 1:
				if (!strstr(buffer, ">Mina avslut idag<"))
					continue;
				n = ftell(fp) - strlen(buffer);
				break;
			}
		}
		if (m == 0 || n == 0) {
			prepare_connection();
			status = order_failed;
			break;
		}
		if (check_avslut(fp, n, action, buffer, sizeof(buffer))) {
			fclose(fp);
			status = order_executed;
			break;
		}
		if (!check_aktuella(fp, m, n, action, buffer, sizeof(buffer))) {
			fclose(fp);
			status = order_killed;
			break;
		}
		fclose(fp);
		sleep(20);
		do {
			const char *url = "https://www.avanza.se/aza/order"
				"/aktie/kopsalj.jsp";
			update_watcher();
			fp = fopen("./OrderResponse.html", "w");
			refresh_conn();
			curl_easy_setopt(conn.handle, CURLOPT_URL, url);
			curl_easy_setopt(conn.handle, CURLOPT_FOLLOWLOCATION, 1);
			curl_easy_setopt(conn.handle, CURLOPT_REFERER, url);
			curl_easy_setopt(conn.handle, CURLOPT_AUTOREFERER, 1);
			curl_easy_setopt(conn.handle, CURLOPT_WRITEDATA, fp);
			err = perform_request();
			fclose(fp);
		} while (err != 0);
	} while (1);
	return status;
}
#endif

enum order_status send_order(enum action_type action)
{
	const struct stock_info *info = get_stock_info(orderbookId);
	struct trade *trade = (struct trade *)g_list_last(market.trades)->data;
	char buffer[1024];
	enum order_status ret;
	const char *actionstr = action == action_buy ? "buy" : "sell";
	GString *gstr = make_valid_price(trade->price);
#if REAL_TRADE

	FILE *fp, *fp1;
	CURLcode err;
	const char *orderurl = "https://www.avanza.se/aza"
		"/order/aktie/kopsalj.jsp";
	assert(info != NULL);
	refresh_conn();
	fp = fopen("./OrderResponse.html", "w");
	fp1 = fopen("./OrderResponse.txt", "w");
	curl_easy_setopt(conn.handle, CURLOPT_HEADERDATA, fp1);
	curl_easy_setopt(conn.handle, CURLOPT_WRITEDATA, fp);
	memset(buffer, 0, sizeof(buffer));
	sprintf(buffer,
		"advanced=true&account=7781011&orderbookId=%s&market=INET&"
		"volume=%ld.0&price=%s&openVolume=0.0&validDate=%s&"
		"condition=AM&orderType=%s&intendedOrderType=%s&"
		"toggleClosings=false&toggleOrderDepth=false&"
		"toggleAdvanced=false&"
		"searchString=%s&transitionId=%s&commit=true&contractSize=1.0&"
		"market=INET&currency=SEK&currencyRate=1.0&countryCode=SE&"
		"orderType=%s&priceMultiplier=1.0&popped=false\n",
		info->orderid, my_position.quantity, gstr->str, get_datestring(),
		actionstr, actionstr, info->name,
		action == action_buy ? "21" : "31", actionstr
		);
	curl_easy_setopt(conn.handle, CURLOPT_COPYPOSTFIELDS, buffer);
	curl_easy_setopt(conn.handle, CURLOPT_REFERER, orderurl);
	curl_easy_setopt(conn.handle, CURLOPT_URL, orderurl);
	curl_easy_setopt(conn.handle, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(conn.handle, CURLOPT_AUTOREFERER, 1);
	if ((err = perform_request())) {
		fclose(fp1);
		fclose(fp);
		g_string_free(gstr, TRUE);
		return order_failed;
	}
	fclose(fp);
	fclose(fp1);

	/* confirm that the order has been executed */
	ret = get_order_status(action);
#else
	ret = order_executed;
#endif
	sprintf(
		buffer,
		"%s to %s %s at %s kronor.\n",
		ret == order_executed ? "succeeded" : "failed",
		actionstr,
		info->name, gstr->str);
	g_string_free(gstr, TRUE);
	declare(buffer);
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
		break;
	case SIGPIPE:
		conn.valid = 0;
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
	if (argc < 4) {
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
	sscanf(argv[3], "%ld", &po.quantity);
	set_position(&po);

	for (i = 0; i < sizeof(cared_signals)/sizeof(cared_signals[0]); i++)
		sigaction(cared_signals[i], &act, NULL);
	srand(time(NULL));
	curl_global_init(CURL_GLOBAL_ALL);
	collect_data();
	curl_global_cleanup();
	return 0;
}
