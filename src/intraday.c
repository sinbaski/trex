#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <mqueue.h>
#include <errno.h>
#include <curl/curl.h>
#include <glib.h>
#include <glib/gprintf.h>
#include "analyze.h"

#define MAX_COOKIE_NUM 6
#define MAX_COOKIE_LEN 100
#define MIN_NEW_TRADES 10
#define DATA_UPDATE_INTERVAL 36
#define TRANSFER_TIMEOUT 60 * 5 /* 5 minutes */
#define WORKDIR "/home/xxie/work/avanza/data_extract/intraday"

CURL *connection_handle = NULL;
extern struct market market;
extern int indicators_initialized(void);
extern void save_indicators(void);
extern void restore_indicators(void);

char cookies[MAX_COOKIE_NUM][MAX_COOKIE_LEN];
char *cookie_container[MAX_COOKIE_NUM + 1];

enum status {
	registering,
	collecting,
	finished
};

volatile enum status my_status;

#if 0
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

static void free_trade (void *p)
{
	g_slice_free(struct trade, p);
	/* fprintf(stderr, "%s: freed %p.\n", __func__, p); */
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

static int extract_data(const char *buffer, struct trade *trade, const GRegex *regex)
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

#if !USE_FAKE_SOURCE
static int find_attr_value(const char *heystack, int len,
			   const char *attr,
			   char **start, int *size)
{
	char *p;
	if (!(p = g_strstr_len(heystack, len, attr)))
		return 0;
	*start = p;
	if (!(p = g_strstr_len(*start, len - (*start - heystack), ";")) &&
	    !(p = g_strstr_len(*start, len - (*start - heystack), "\r")) &&
	    !(p = g_strstr_len(*start, len - (*start - heystack), "\n")))
		*size = len - (*start - heystack);
	else
		*size = p - *start;
	return 1;
}

static void set_cookie(const char *start, int size)
{
	const char *p, *q;
	char *cookie;
	int len, x, i;
	const char *attributes[] = {
		"Version", "Path", "Domain"
	};

	for (p = start, i = 0; i < size && *p != '='; p++);
	if (i == size) {
		fprintf(stderr, "Invalid Cookie.\n");
		return;
	}
	/* strncpy(name, start, len = p - start); */
	for (i = 0; i < MAX_COOKIE_NUM && cookies[i][0] &&
		     strncmp(start, cookies[i], len); i++);
	if (i == MAX_COOKIE_NUM) {
		fprintf(stderr, "No space for more cookies.\n");
		return;
	}
	cookie = cookies[i];
	if (!(q = g_strstr_len(p, size - len - 1, ";"))) {
		/* No attributes */
		memcpy(cookie, start, size);
		cookie[size] = 0;
		return;
	}
	memcpy(cookie, start, x = q - start);
	/* Attributes */
	for (i = 0; i < sizeof(attributes)/sizeof(char *); i++) {
		char *cp = NULL;
		if (find_attr_value(q + 1, size - (q - start),
				    attributes[i], &cp, &len)) {
			if (x >= MAX_COOKIE_LEN) {
				fprintf(stderr, "The cookie is too long:\n");
				cookie[MAX_COOKIE_LEN] = 0;
				return;
			}
			memcpy(cookie + x, "; ", 2);
			memcpy(cookie + x + 2, cp, len);
			x += len + 2;
		}
	}
	cookie[x] = 0;
}

static size_t write_header(void *buffer, size_t size, size_t nmemb, void *userp)
{
	const char *cookiestr = "Set-Cookie:";
	char *chp;
	char * start = (char *)buffer;
	int len = strlen(cookiestr);
	int cap = size * nmemb;

	if (cap < len)
		return cap;
	if (strncmp((char *)buffer, cookiestr, len))
		return cap;
	for (chp = start + len; *chp == ' '; chp++);
	set_cookie(chp, cap - (chp -(char *)buffer));
	return cap;
}

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	int cap = size * nmemb;
	switch (g_atomic_int_get(&my_status)) {
	case registering:
		break;
	case collecting: {
		FILE *fp = (FILE *)userp;
		fwrite(buffer, cap, 1, fp);
		break;
	}
	default:
		break;
	}
	return cap;
}
#endif

static int login(CURL *handle)
{
	int ret = 0;
#if !USE_FAKE_SOURCE
	int code;
	int i;
	char *headerlines[] = {
		"Host: www.avanza.se",
		"Connection: keep-alive",
		"Referer: https://www.avanza.se/aza/home/home.jsp",
		"Content-Length: 37",
		"Cache-Control: max-age=0",
		"Origin: https://www.avanza.se",
		"User-Agent: Mozilla/5.0 (Windows NT 5.1) "
		"Content-Type: application/x-www-form-urlencoded",
		"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
		"Accept-Encoding: gzip,deflate",
		"Accept-Language: en-us,en;q=0.5",
		"Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7",
	};
	struct curl_slist *headers = NULL;
	const char *loginfo = "username=sinbaski&password=2Oceans%3F";
	const char *url ="https://www.avanza.se/aza/login/login.jsp";

	char error_buffer[CURL_ERROR_SIZE];

	for (i = 0; i < sizeof(headerlines)/sizeof(char *); i++) {
		headers = curl_slist_append(headers, headerlines[i]);		
	}
	if ((code = curl_easy_setopt(handle, CURLOPT_ERRORBUFFER,
				     error_buffer))) {
		fprintf(stderr, "Failed to set CURLOPT_ERRORBUFFER. Error %d.\n", code);
		ret = code;
		goto end;
	}
	if ((code = curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers))) {
		fprintf(stderr, "Failed to set CURLOPT_HTTPHEADER. Error %d.\n", code);
		ret = code;
		goto end;
	}
	if ((code = curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION,
				     write_header))) {
		fprintf(stderr, "curl_easy_setopt failed with "
			"CURLOPT_WRITEDATA. Error %d.\n",
			code);
		goto end;
	}
	if ((code = curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION,
				     write_data))) {
		fprintf(stderr, "curl_easy_setopt failed with "
			"CURLOPT_WRITEFUNCTION. Error %d.\n",
			code);
		goto end;
	}
	if ((code = curl_easy_setopt(handle, CURLOPT_POSTFIELDS, loginfo))) {
		fprintf(stderr, "Failed to set CURLOPT_POSTFIELDS. Error %d.\n", code);
		ret = code;
		goto end;
	}
	if ((code = curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, strlen(loginfo)))) {
		fprintf(stderr, "Failed to set CURLOPT_POSTFIELDSIZE. Error %d.\n", code);
		ret = code;
		goto end;
	}
	if ((code = curl_easy_setopt(handle, CURLOPT_URL, url))) {
		fprintf(stderr, "Failed to set CURLOPT_URL. Error %d.\n", code);
		ret = code;
		goto end;
	}
	g_atomic_int_set(&my_status, registering);
	if ((code = curl_easy_perform(handle))) {
		fprintf(stderr, "[%s] %s: curl_easy_perform() failed. "
			"Error %d: %s\n", get_timestring(), __func__,
			code, error_buffer);
		sleep(100 + rand() % 140); 
		ret = code;
		goto end;
	}
end:
	curl_slist_free_all(headers);
	fflush(stderr);
#else
	g_atomic_int_set(&my_status, registering);	
#endif
	return ret;

}

static int prepare_http_headers(CURL *handle, struct curl_slist **headers)
{

	int i;
	int code, ret = 0;
	char temp[200];
	char *joined_cookies = NULL;
	char *cookie = NULL;
	char *headerlines[] = {
		"Host: www.avanza.se",
		"Connection: keep-alive",
		"User-Agent: Mozilla/5.0 (Windows NT 5.1) "
		"AppleWebKit/535.1 (KHTML, like Gecko) Chrome/13.0.782.107 Safari/535.1",
		"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
		"Accept-Encoding: gzip,deflate",
		"Accept-Language: en-us,en;q=0.5",
		"Accept-Charset: GBK,utf-8;q=0.7,*;q=0.3",
		temp
	};
	sprintf(temp, "Referer: https://www.avanza.se"
		"/aza/aktieroptioner/kurslistor/aktie.jsp"
		"?orderbookId=%s&grtype=intraday&sort=byID",
		orderbookId);
	for (i = 0; i < sizeof(headerlines)/sizeof(char *); i++) {
		*headers = curl_slist_append(*headers, headerlines[i]);		
	}
	for (i = 0; i < MAX_COOKIE_NUM && cookies[i][0]; i++)
		cookie_container[i] = cookies[i];
	cookie_container[i] = NULL;
	joined_cookies = g_strjoinv("; ", cookie_container);
	cookie = g_strjoin("", "Cookie: ", joined_cookies, NULL);
	g_free(joined_cookies);
	*headers = curl_slist_append(*headers, cookie);
	g_free(cookie);
	if ((code = curl_easy_setopt(handle, CURLOPT_HTTPHEADER, *headers))) {
		fprintf(stderr, "Failed to set CURLOPT_HTTPHEADER. "
			"Error %d.\n", code);
		ret = code;
		goto end;
	}
	sprintf(temp,
		"https://www.avanza.se/aza/aktieroptioner/"
		"kurslistor/avslut.jsp?"
		"password=2Oceans?&orderbookId=%s&username=sinbaski",
		orderbookId);
	if ((code = curl_easy_setopt(handle, CURLOPT_URL, temp))) {
		fprintf(stderr, "Failed to set CURLOPT_HTTPHEADER. "
			"Error %d.\n", code);
		ret = code;
		goto end;
	}
end:
	fflush(stderr);
	return ret;
}

static void prepare_connection(char *error_buffer, struct curl_slist **headers)
{
	CURLcode code;
	CURL *handle = NULL;
	if (connection_handle) {
		curl_easy_cleanup(connection_handle);
		curl_global_cleanup();
	}
	memset(&cookies[0][0], 0, MAX_COOKIE_NUM * MAX_COOKIE_LEN);
	while (!handle) {
		if ((code = curl_global_init(CURL_GLOBAL_ALL))) {
			fprintf(stderr, "curl_global_init failed "
				"with %u\n", code);
			goto end0;
		}
		if (!(handle = curl_easy_init())) {
			fprintf(stderr, "curl_easy_init() failed.\n");
			goto end1;
		}
		if ((code = login(handle))) {
			goto end2;
		}
		if ((code = prepare_http_headers(handle, headers))) {
			goto end2;
		}
		if ((code = curl_easy_setopt(handle, CURLOPT_ERRORBUFFER,
					     error_buffer))) {
			goto end2;
		}
		if ((code = curl_easy_setopt(handle, CURLOPT_TIMEOUT,
					     TRANSFER_TIMEOUT))) {
			goto end2;
		}
		break;

	end2:
		curl_easy_cleanup(handle);
		handle = NULL;
		fflush(stderr);
	end1:
		curl_global_cleanup();
	end0:
		continue;
	}
	connection_handle = handle;
	g_atomic_int_set(&my_status, collecting);
}

static void collect_data(void)
{
	GRegex *regex;
	GError *error = NULL;
	char error_buffer[CURL_ERROR_SIZE];
	struct curl_slist *headers = NULL;
	int i;
	regex = g_regex_new("^<TR.*><TD.*>[^<]*</TD>"
			    "<TD.*>[^<]*</TD>"
			    "<TD.*>([A-Z]+)</TD>"
			    "<TD.*>([0-9]{2}:[0-9]{2}:[0-9]{2})</TD>"
			    "<TD.*>([0-9]+,[0-9]{2})</TD>"
			    "<TD.*>([0-9]+)</TD></TR>$",
			    0, 0, &error);

	prepare_connection(error_buffer, &headers);
	for (i = 0; g_atomic_int_get(&my_status) == collecting; i = 1) {
		struct stat st;
		FILE *fp;
#if !USE_FAKE_SOURCE
		CURLcode code;
		fp = fopen("./intraday.html", "w");
		if (i == 0)
			memset(&market, 0, sizeof(market));
		if ((code = curl_easy_setopt(connection_handle, CURLOPT_WRITEDATA, fp))) {
			fprintf(stderr, "curl_easy_setopt failed with CURLOPT_WRITEDATA. Error %d.\n",
				code);
			fclose(fp);
			continue;
		}
		if ((code = curl_easy_perform(connection_handle))) {
			char *timestring;
			timestring = get_timestring();
			fprintf(stderr, "[%s] %s: curl_easy_perform() failed. "
				"Error %d: %s\n", timestring, __func__,
				code, error_buffer);
			fflush(stderr);
			sleep(100 + rand() % 140);
			prepare_connection(error_buffer, &headers);
			fclose(fp);
			continue;
		}
		fclose(fp);
#else
		FILE *req, *resp;
		char message[10];
		/* Request data */
		req = fopen("./req-fifo", "w");
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
		sleep(DATA_UPDATE_INTERVAL);
#endif
	}
	curl_easy_cleanup(connection_handle);
	curl_global_cleanup();
	curl_slist_free_all(headers);
	g_list_free_full(market.trades, free_trade);
	g_regex_unref(regex);
	fclose(stdout);
	fclose(stderr);
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
		SIGHUP, SIGINT, SIGPIPE,
		SIGTERM, SIGUSR1, SIGUSR2, SIGPOLL,
		SIGPROF, SIGSTKFLT, SIGIO, SIGPWR,
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
#if 0
	daemonize();
	for (time(&now); localtime(&now)->tm_hour < 9; time(&now))
		sleep(DATA_UPDATE_INTERVAL);
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
