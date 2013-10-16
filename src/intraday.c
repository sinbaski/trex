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
#include <tidy.h>
#include <buffio.h>
#include <mysql.h>
#include "analyze.h"
#include "utilities.h"

#define MIN_NEW_TRADES 5
#define TRANSFER_TIMEOUT 60 * 5 /* 5 minutes */
#define WORKDIR "/home/xxie/work/avanza/data_extract/intraday"
#define COOKIE_FILE "./cookies.txt"
#define DATA_UPDATE_INTERVAL 36

struct market mkt;

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
	NULL, "", 0
};

struct stock_info stockinfo;

char user[16];
char password[32];

char todays_date[11];
MYSQL *mysqldb;
/* A date string indicating the data used for calibration */
struct trade_flags my_flags;
struct trade_instruction instruction = {
	action_none, 0
};
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
	/* FILE *fp = fopen("./beep", "a"); */
	/* fprintf(fp, "%s\n", str); */
	/* fclose(fp); */
	return;
}

static void load_trade_data(void)
{
	MYSQL_RES *res;
	MYSQL_ROW row;
	char stmt[256];

	sprintf(stmt, "select name, tbl_name, orderid, my_mode, my_status, "
		"my_price, my_quantity, my_quota from company "
		"where dataid=\"%s\";", stockinfo.dataid);
	if (mysql_query(mysqldb, stmt)) {
		fprintf(stderr, "mysql error: %s\n", mysql_error(mysqldb));
		return;
	}
	res = mysql_store_result(mysqldb);
	row = mysql_fetch_row(res);
	strcpy(stockinfo.name, row[0]);
	strcpy(stockinfo.tbl_name, row[1]);
	strcpy(stockinfo.orderid, row[2]);
	sscanf(row[3], "%d", (int*)&my_position.mode);
	sscanf(row[4], "%d", (int*)&my_position.status);
	sscanf(row[5], "%lf", &my_position.price);
	sscanf(row[6], "%ld", &my_position.quantity);
	sscanf(row[7], "%lf", &my_position.quota);
	mysql_free_result(res);

	if (my_position.status == 1)
		strcpy(my_position.time, "00:00:00");
}

time_t get_trade(MYSQL *db, long n, struct trade *trade1)
{
	/* long offset = ftell(datafile); */
	struct trade trade2;
	struct trade *trade = trade1 ? trade1 : &trade2;

	MYSQL_RES *res;
	MYSQL_ROW row;
	char stmt[128];
	sprintf(stmt, "select price, volume, time(tid) from %s "
			"where tid like '%s %%' order by tid desc limit %ld, 1;",
			stockinfo.tbl_name, todays_date, n);
	if (mysql_query(mysqldb, stmt)) {
		fprintf(stderr, "mysql error: %s\n", mysql_error(mysqldb));
		return 0;
	}
	res = mysql_store_result(mysqldb);

	row = mysql_fetch_row(res);
	sscanf(row[0], "%lf", &trade->price);
	sscanf(row[1], "%ld", &trade->quantity);
	strncpy(trade->time, row[2], sizeof(trade->time));
	mysql_free_result(res);
	return parse_time(trade->time);
}

static void free_trade (void *p)
{
	g_slice_free(struct trade, p);
}

int trade_equal(const struct trade *t1, const struct trade *t2)
{
	const char *ticksize1, *ticksize2;
	double x;

	ticksize1 = get_tick_size(t1->price);
	ticksize2 = get_tick_size(t2->price);
	if (strcmp(ticksize1, ticksize2) != 0)
		return 0;
	sscanf(ticksize1, "%lf", &x);
	if (abs(t1->price - t2->price) >= x)
		return 0;
	return strcmp(t1->buyer, t2->buyer) == 0 &&
		strcmp(t1->seller, t2->seller) == 0 &&
		strcmp(t1->time, t2->time) == 0 &&
		t1->quantity == t2->quantity;
}

int get_num_records(const char *date)
{
	MYSQL_RES *res;
	MYSQL_ROW row;
	const char *tbl_name = stockinfo.tbl_name;
	char stmt[128];
	int num_rows;

	sprintf(stmt, "select count(*) from %s "
			"where tid like '%s %%';", tbl_name, date);
	if (mysql_query(mysqldb, stmt)) {
		fprintf(stderr, "mysql error: %s\n", mysql_error(mysqldb));
		return -1;
	}
	res = mysql_store_result(mysqldb);
	row = mysql_fetch_row(res);
	sscanf(row[0], "%d", &num_rows);
	mysql_free_result(res);
	return num_rows;
}

static int log_data(int idx)
{
#if !USE_FAKE_SOURCE
	const GList *node = g_list_nth(mkt.trades, idx);
	GString *gstr = g_string_sized_new(128);
	struct trade last;
	static int check_redundancy = 1;
	int n = 0;

	if (check_redundancy) {
		int l = get_num_records(todays_date);
		if (l == 0)
			check_redundancy = 0;
		else if (l < 0)
			return 0;
		else
			get_trade(mysqldb, 0, &last);
	}
	/* datafile = fopen(get_filename("records", ".dat"), "a"); */
	g_string_printf(gstr, "insert into %s values ", stockinfo.tbl_name);
	n = 0;
	while (node) {
		const struct trade *trade =
			(struct trade *)node->data;
		if (!check_redundancy || strcmp(trade->time, last.time) > 0) {
			g_string_append_printf(
				gstr, "('%s', '%s', %7.2lf, %ld, '%s %s'),",
				trade->buyer, trade->seller,
				trade->price, trade->quantity,
				todays_date, trade->time);
			/* fprintf(datafile, "%6s\t%8s\t%7.2lf\t%7ld\n", */
			/* 	trade->mkt, trade->time, */
			/* 	trade->price, trade->quantity); */
			check_redundancy = 0;
			n++;
		}
		node = node->next;
	}
	if (n == 0)
		goto end;
	gstr->str[gstr->len - 1] = ';';
	if (mysql_query(mysqldb, gstr->str)) {
		fprintf(stderr, "mysql error: %s\n", mysql_error(mysqldb));
		return 0;
	}
end:
	g_string_free(gstr, TRUE);
	return n;
#else
	return mkt.new_trades;
#endif
}

#if !USE_FAKE_SOURCE
static void refresh_conn(void)
{
	curl_easy_reset(conn.handle);
	curl_easy_setopt(conn.handle, CURLOPT_ERRORBUFFER, conn.errbuf);
	curl_easy_setopt(conn.handle, CURLOPT_TIMEOUT, TRANSFER_TIMEOUT);
	curl_easy_setopt(conn.handle, CURLOPT_USERAGENT,
			 "Mozilla/5.0 (compatible; MSIE 9.0; "
			 "Windows NT 6.1; WOW64; Trident/5.0)");
	if (g_atomic_int_get(&my_status) != registering) {
		char buffer[32];
		sprintf(buffer, "cookies-%s.txt", stockinfo.dataid);
		curl_easy_setopt(conn.handle, CURLOPT_COOKIEFILE, buffer);
	}
}
#endif

void discard_old_records(int size)
{
	int i;
	for (i = 0; i < size; i++) {
		g_slice_free(struct trade, mkt.trades->data);
		mkt.trades = g_list_delete_link(mkt.trades,
						   mkt.trades);
	}
	fflush(stderr);
}

static void get_trade_instruction(void)
{
	MYSQL_RES *res;
	MYSQL_ROW row;
	char stmt[256];
	const char *now;
	int status;

	now = get_timestring();
	instruction.action = action_none;
	sprintf(stmt, "select action, price, status, time(expiration) "
		"from instructions where co_name=\"%s\" and "
		"date(expiration) = \"%s\" order by expiration;",
		stockinfo.tbl_name, todays_date);
	if (mysql_query(mysqldb, stmt)) {
		fprintf(stderr, "mysql error: %s\n", mysql_error(mysqldb));
		return;
	}
	res = mysql_store_result(mysqldb);
	while ((row = mysql_fetch_row(res))) {
		sscanf(row[0], "%d", (int*)&instruction.action);
		sscanf(row[1], "%lf", &instruction.price);
		sscanf(row[2], "%d", &status);
		strcpy(instruction.expiration, row[3]);

		if (status == 0 && strcmp(now, row[3]) < 0)
			break;
		else if (status == 0 && strcmp(now, row[3]) > 0) {
			instruction.action = action_none;
			break;
		}
	}
	mysql_free_result(res);
}

static char *strsub(char *str, char c1, char c2)
{
	char *p;
	int l = strlen(str);
	for (p = str; p - str < l; p++) {
		if (*p == c1) *p = c2;
	}
	return str;
}

static int get_node_cardinality(TidyNode tnode)
{
	int n;
	TidyNode node;
	for (n = 0, node = tnode; node; node = tidyGetPrev(node), n++);
	return n - 1;
}

static char *str_rm_html_char(char *str)
{
	char *p, *q;
	for (p = strchr(str, '&'), q = strchr(str, ';');
	     p && q && q > p;) {
	
		strcpy(p, q + 1);
		if (*p && (p = strchr(p, '&'))) {
			q = strchr(p, ';');
		}
		else q = NULL;
	}
	return str;
}
static int walk_doc_tree(TidyDoc tdoc, TidyNode tnode,
			  int *state, struct trade *trade)
{
	TidyNode child;
	static int gp;

	if (*state == 0) gp = g_list_length(mkt.trades);

	for (child = tidyGetChild(tnode); child && state >= 0;
	     child = tidyGetNext(child)) {
		ctmbstr name = tidyNodeGetName( child );
		TidyBuffer buf;
		if (*state == 0 && name && strcmp(name, "tbody")) {
			walk_doc_tree(tdoc, child, state, trade);
		} else if (*state == 0 && name && strcmp(name, "tbody") == 0) {
			*state = 1;
			walk_doc_tree(tdoc, child, state, trade);
			*state = -1;
		} else if (*state == 1 && name && strcmp(name, "tr") == 0) {
			struct trade *p;
			memset(trade, 0, sizeof(*trade));
			*state = 2;
			walk_doc_tree(tdoc, child, state, trade);
			*state = 1;
			if (trade->quantity == 0)
				continue;
			if (gp) {
				struct trade *trade2 = (struct trade *)
					g_list_nth_data(mkt.trades, gp - 1);
				if (abs(trade->price - trade2->price)/trade2->price > 2.0e-2)
					continue;
				if (trade_equal(trade, trade2)) {
					*state = -1;
					continue;
				}
			}
			p = g_slice_dup(struct trade, trade);
			mkt.trades = g_list_insert(mkt.trades, p, gp);
			mkt.new_trades++;
		} else if (*state == 2 && name && strcmp(name, "td") == 0) {
			*state = 3;
			walk_doc_tree(tdoc, child, state, trade);
			*state = 2;
		} else if (*state >= 3 && name) {
			int i = *state;
			(*state)++;
			walk_doc_tree(tdoc, child, state, trade);
			*state = i;
		} else if (*state >= 3 && !name) {
			TidyNode node = tnode;
			while (!tidyNodeIsTD(node)) node = tidyGetParent(node);

			tidyBufInit(&buf);
			tidyNodeGetText(tdoc, child, &buf);
			
			switch (get_node_cardinality(node))
			{
			case 0:
				sscanf(g_strstrip((char *)buf.bp), "%s",
				       trade->buyer);
				break;
			case 1:
				sscanf(g_strstrip((char *)buf.bp), "%s",
				       trade->seller);
				break;
			case 2: 
			{
				char *str;
				str = g_strstrip((char *)buf.bp);
				str = str_rm_html_char(str);
				sscanf(str, "%ld", &trade->quantity);
				break;
			}
			case 3:
			{
				char *str = strsub(g_strstrip((char *)buf.bp),
						   ',', '.');
				sscanf(str, "%lf", &trade->price);
				break;
			}
			case 4:
				sscanf(g_strstrip((char *)buf.bp), "%s",
				       trade->time);
				break;

			}
			tidyBufFree(&buf);
		}
	}
	/* if (*state >= 0) *state = s; */
	return gp;
}

static void refine_data(const char* xchgfile)
{
	/* CURL *curl; */
	/* char curl_errbuf[CURL_ERROR_SIZE]; */
	TidyDoc tdoc;
	TidyBuffer docbuf = {0};
	TidyBuffer tidy_errbuf = {0};
	int err;
	int state, gp;
	int l;
	gchar *contents;
	gsize flen;
	GError *gerr;
	struct trade trade;

	tdoc = tidyCreate();
	tidyOptSetBool(tdoc, TidyForceOutput, yes); /* try harder */
	tidyOptSetInt(tdoc, TidyWrapLen, 4096);
	tidySetErrorBuffer( tdoc, &tidy_errbuf );
	tidyBufInit(&docbuf);

	g_file_get_contents(xchgfile, &contents, &flen, &gerr);
	tidyBufAppend(&docbuf, contents, flen);
	err = tidyParseBuffer(tdoc, &docbuf); /* parse the input */
	if (err < 0) {
		fprintf(stderr, "Failed to parse server response. err %d", err);
		goto cleanup;
	}
	err = tidyCleanAndRepair(tdoc); /* fix any problems */
	if ( err < 0 ) goto cleanup;

	err = tidyRunDiagnostics(tdoc); /* load tidy error buffer */
	if ( err < 0 ) goto cleanup;

	state = 0;
	gp = walk_doc_tree(tdoc, tidyGetRoot(tdoc), &state, &trade);

	get_trade_instruction();
	if (instruction.action != action_none) {
		if (mkt.new_trades) {
			log_data(gp);
			mkt.new_trades = 0;
		}
		if (execute(instruction.action, instruction.price)
		    == order_executed) {
			char buffer[128];
			sprintf(buffer, "update instructions set status = 1 "
				"where co_name=\"%s\" and "
				"expiration = \"%s %s\";",
				stockinfo.tbl_name, todays_date,
				instruction.expiration);
			if (mysql_query(mysqldb, buffer)) {
				fprintf(stderr, "mysql error: %s\n",
					mysql_error(mysqldb));
			}
		}
	}
	else if (mkt.new_trades == 0 || log_data(gp) == 0)
		goto cleanup;
	else if (mkt.new_trades >= MIN_NEW_TRADES) {
		/* analyze(); */
		mkt.new_trades = 0;
	}
	l = g_list_length(mkt.trades) - MAX_TRADES_COUNT;
	if (l > 0)
		discard_old_records(l);

cleanup:
	g_free(contents);
	tidyBufFree(&docbuf);
	tidyBufFree(&tidy_errbuf);
	tidyRelease(tdoc);
	return;
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

#if !USE_FAKE_SOURCE
static int login(void)
{
	int ret = 0;
	int code;
	/* const char *loginfo = "username=sinbaski&password=2Oceans?"; */
	char loginfo[80], buffer[32];
	sprintf(loginfo, "j_username=%s&j_password=%s", user, password);
	FILE *fp, *body_fp;
	g_atomic_int_set(&my_status, registering);

	refresh_conn();
	curl_easy_setopt(conn.handle, CURLOPT_POSTFIELDS, loginfo);

	sprintf(buffer, "https://www.avanza.se/start");
	curl_easy_setopt(conn.handle, CURLOPT_REFERER, buffer);
	sprintf(buffer, "https://www.avanza.se/ab/noop");
	curl_easy_setopt(conn.handle, CURLOPT_URL, buffer);

	sprintf(buffer, "cookies-%s.txt", stockinfo.dataid);
	fp = fopen(buffer, "w");
	curl_easy_setopt(conn.handle, CURLOPT_HEADERDATA, fp);
	curl_easy_setopt(conn.handle, CURLOPT_HEADERFUNCTION, store_cookie);
	/* The body data are not needed following the login post */
	sprintf(buffer, "./login-%s.html", stockinfo.dataid);
	body_fp = fopen(buffer, "w");
	curl_easy_setopt(conn.handle, CURLOPT_WRITEDATA, body_fp);
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
	fclose(body_fp);
	/* curl_free(str); */
end:
	fflush(stderr);
	return ret;
}
#endif

static void prepare_connection(void)
{
#if !USE_FAKE_SOURCE
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
#else
	g_atomic_int_set(&my_status, collecting);
#endif
}

#if REAL_TRADE || !USE_FAKE_SOURCE
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
	sleep(100 + rand() % 140);
	prepare_connection();
	return code;
}
#endif

static inline void update_watcher(void)
{
	char buf[20];
	FILE *fp;
	sprintf(buf, "watcher-%s", stockinfo.dataid);
	fp = fopen(buf, "w");
	fprintf(fp, "Active\n");
	fclose(fp);
}

static void collect_data(void)
{
	GRegex *regex;
	GError *error = NULL;
	GString *gstr = g_string_sized_new(128);
	char xchgfile[100];
	int i;
#if USE_FAKE_SOURCE
	FILE *req, *resp;
	char message[10];
#endif
	/* The number of shares that we have on the account */
	regex = g_regex_new("^<TR.*><TD.*><A.*>([A-Z]+)</A></TD>"
			    "<TD.*><A.*>([A-Z]+)</A></TD>"
			    "<TD.*>([A-Z]+)</TD>"
			    "<TD.*>([0-9]{2}:[0-9]{2}:[0-9]{2})</TD>"
			    "<TD.*>([0-9]+,[0-9]{2})</TD>"
			    "<TD.*>([0-9]+)</TD></TR>$",
			    (GRegexCompileFlags)0, (GRegexMatchFlags)0,
			    &error);
	update_watcher();
	prepare_connection();

	sprintf(xchgfile, "./intraday-%s.html", stockinfo.dataid);
	/* if (get_status(&my_position.status, &my_position.price) != 0) */
	/* 	return; */
	for (i = 0; g_atomic_int_get(&my_status) == collecting; i = 1) {
		struct stat st;
		FILE *fp;
#if !USE_FAKE_SOURCE
		time_t t1, t2;
#endif
		/* Report to the watcher that I am active */
		update_watcher();
		if (i == 0)
			memset(&mkt, 0, sizeof(mkt));

#if !USE_FAKE_SOURCE
		fp = fopen(xchgfile, "w");
		time(&t1);
		refresh_conn();
		curl_easy_setopt(conn.handle, CURLOPT_HTTPGET, 1);
		curl_easy_setopt(conn.handle, CURLOPT_WRITEDATA, fp);
		g_string_printf(gstr, "https://www.avanza.se/aktier"
				"/dagens-avslut.html/%s/%s",
				stockinfo.dataid, stockinfo.name);
		curl_easy_setopt(conn.handle, CURLOPT_URL, gstr->str);

		g_string_printf(gstr, "https://www.avanza.se/aktier/"
				"om-aktien.html/%s/%s", stockinfo.dataid,
				stockinfo.name);
		curl_easy_setopt(conn.handle, CURLOPT_REFERER, gstr->str);
		if (perform_request()) {
			fclose(fp);
			continue;
		}
		fclose(fp);
#else
		/* Request data */
		req = fopen("./req-fifo", "w");
		if (!req) {
			my_status = finished;
			fprintf(stderr, "Cannot open ./req-fifo. Quit.\n");
			continue;
		}
		fprintf(req, "get");
		fclose(req);
		/* Wait until the data is returned */
		resp = fopen("./resp-fifo", "r");
		fscanf(resp, "%s", message);
		fclose(resp);
#endif
		stat(xchgfile, &st);
		if (st.st_size) {
			/* fp = fopen(xchgfile, "r"); */
			/* refine_data(fp, regex); */
			refine_data(xchgfile);
			/* fclose(fp); */
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
#else
		if (strncmp(message, "done", sizeof(message)) == 0)
			g_atomic_int_set(&my_status, finished);
#endif
	}
	curl_easy_cleanup(conn.handle);
	curl_global_cleanup();
	g_list_free_full(mkt.trades, free_trade);
	g_regex_unref(regex);
	g_string_free(gstr, TRUE);
	gstr = NULL;
#if USE_FAKE_SOURCE
	if (strcmp(message, "done") != 0) {
		req = fopen("./req-fifo", "w");
		fprintf(req, "bye");
		fclose(req);
	}
#endif
	analyzer_cleanup();
}

#if REAL_TRADE
static int check_aktuella(FILE *fp, long m, long n, enum action_type action,
			  char buffer[], int size)
{
	int ret = 0;
	fseek(fp, m, SEEK_SET);
	while (!ret && ftell(fp) < n && fgets(buffer, size, fp)) {
		if (strstr(buffer, stockinfo.name)) ret = 1;
	}
	return ret;
}

static int check_avslut(FILE *fp, long n, enum action_type action,
			char *buffer, int size)
{
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
			} else if (strstr(buffer, stockinfo.name)) {
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

enum order_status send_order(enum action_type action, const char *price)
{
	char buffer[1024];
	enum order_status ret;
	const char *actionstr = action == action_buy ? "buy" : "sell";

	const char *statusstr[] = {
		"executed",
		"killed",
		"unsuccessful"
	};
#if REAL_TRADE

	FILE *fp, *fp1;
	CURLcode err;
	const char *orderurl = "https://www.avanza.se/aza"
		"/order/aktie/kopsalj.jsp";
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
		stockinfo.orderid, my_position.quantity, price, get_datestring(),
		actionstr, actionstr, stockinfo.name,
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
		/* "%s to %s %s at %s kronor.\n", */
		"The order to %s %s at %s is %s.\n",
		actionstr, stockinfo.name, price, statusstr[ret]);
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

int main(int argc, char *argv[])
{
	struct sigaction act;
	int cared_signals[] = {
		SIGTERM, SIGPIPE
	};
	struct rlimit rlim = {
		RLIM_INFINITY, RLIM_INFINITY
	};
	unsigned int i;
	int opt;

	act.sa_sigaction = signal_handler;
	act.sa_flags = SA_SIGINFO;

	memset(&my_position, 0, sizeof(my_position));
	memset(&my_flags, 0, sizeof(my_flags));
	while ((opt = getopt(argc, argv, "s:w:d:n:u:p:")) != -1) {
               switch (opt) {
               case 's':
		       strcpy(stockinfo.dataid, optarg);
		       break;
	       case 'w':
		       sscanf(optarg, "%d", &i);
		       my_flags.do_trade = i;
		       break;
	       case 'd':
		       strcpy(todays_date, optarg);
		       break;
	       case 'n':
		       sscanf(optarg, "%d", &i);
		       my_flags.allow_new_entry = i;
		       break;
	       case 'u':
		       sscanf(optarg, "%s", user);
		       break;
	       case 'p':
		       sscanf(optarg, "%s", password);
		       break;
               default: /* '?' */
                   fprintf(stderr, "Usage: %s -s stock -m mode "
			   "-q quantity [-p enter_price]\n",
                           argv[0]);
                   exit(EXIT_FAILURE);
               }
	}
	if (strlen(stockinfo.dataid) == 0) {
                   fprintf(stderr, "Usage: %s -s stock -w trade? "
			   "-n new? -d date\n", argv[0]);
		   exit(EXIT_FAILURE);
	}
#if DAEMONIZE
	daemonize();
#endif
	freopen( "/dev/null", "r", stdin);
	freopen(get_filename("transactions", ".txt"), "a", stdout);
	freopen(get_filename("logs", ".log"), "a", stderr);

	setrlimit(RLIMIT_CORE, &rlim);

	for (i = 0; i < sizeof(cared_signals)/sizeof(cared_signals[0]); i++)
		sigaction(cared_signals[i], &act, NULL);
	srand(time(NULL));
	curl_global_init(CURL_GLOBAL_ALL);
	mysqldb = mysql_init(NULL);
	if (!mysql_real_connect(mysqldb, "localhost", "sinbaski", "q1w2e3r4",
				"avanza", 0, NULL, 0)) {
		fprintf(stderr, "%s\n", mysql_error(mysqldb));
		goto end1;
	}
	mysql_autocommit(mysqldb, 1);
	load_trade_data();
	collect_data();
	mysql_close(mysqldb);
end1:
	curl_global_cleanup();

	return 0;
}
