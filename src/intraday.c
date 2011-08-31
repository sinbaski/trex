#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <curl/curl.h>
#include <glib.h>
#include <glib/gprintf.h>

#include "analyze.h"

#define MAX_COOKIE_NUM 6
#define MAX_COOKIE_LEN 100
#define MIN_NEW_TRADES 10
#define DATA_UPDATE_INTERVAL 30
#define WORKDIR "/home/xxie/work/avanza/data_extract/intraday"

const char *orderbookId = NULL;
extern struct market market;
extern int indicators_initialized(void);

char cookies[MAX_COOKIE_NUM][MAX_COOKIE_LEN];
char *cookie_container[MAX_COOKIE_NUM + 1];


enum status {
	registering,
	collecting,
	finished
};

volatile enum status my_status;

static void signal_handler(int sig)
{
	switch (sig) {
	case SIGTERM:
		g_atomic_int_set(&my_status, finished);
		fprintf(stderr, "%s: SIGTERM received.\n", __func__);
		break;
		/* notify the collector and the analyzer to stop */
	default:;
	}
}

#ifndef DEBUG
static void daemonize(void)
{
	pid_t pid, sid;
	time_t today;
	struct tm *timep;
	char name[50];

	time(&today);
	timep = localtime(&today);

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

	/* Redirect standard files to /dev/null */
	freopen( "/dev/null", "r", stdin);
	sprintf(name, "transactions/%s-%d-%d-%d.dat", orderbookId,
		timep->tm_year + 1900, timep->tm_mon + 1,
		timep->tm_mday);
	freopen(name, "w", stdout);
	sprintf(name, "logs/%s-%d-%d-%d.log", orderbookId,
		timep->tm_year + 1900, timep->tm_mon + 1,
		timep->tm_mday);
	freopen(name, "w", stderr);
}
#endif

static void free_trade (void *p)
{
	g_slice_free(struct trade, p);
}

static void log_data(FILE *datafile, const GList *node)
{
	while (node) {
		const struct trade *trade =
			(struct trade *)node->data;
		fprintf(datafile, "%6s\t%8s\t%8.2lf\t%5ld\n",
			trade->market, trade->time,
			trade->price, trade->quantity);
		node = node->next;
	}
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

static void discard_old_records(void)
{
	int i, l;
	for (i = 0, l = market.trades_count - BATCH_SIZE; i < l; i++) {
		g_slice_free(struct trade, market.trades->data);
		market.trades = g_list_delete_link(market.trades,
						   market.trades);
	}
	market.trades_count = BATCH_SIZE;
}

static FILE *open_data_file(void)
{
	time_t today;
	struct tm *timep;
	char name[50];

	time(&today);
	timep = localtime(&today);
	sprintf(name, "records/%s-%d-%d-%d.dat", orderbookId,
		timep->tm_year + 1900, timep->tm_mon + 1,
		timep->tm_mday);
	return fopen(name, "a");
}
static void refine_data(FILE *fp, FILE *datafile, const GRegex *regex)
{
	char buffer[1000];
	enum {
		before, between, within, after
	} position = before;

	while (fgets(buffer, sizeof(buffer), fp) &&
		position != after) {
		int len = strlen(buffer);

		switch (position) {
		case before:
			if (g_strstr_len(buffer, len, "Avslut "))
				position = between;
			break;
		case between:
			if (!g_str_has_prefix(buffer, "<TR><TD "))
				break;
			/* Fall through */
			position = within;
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
				market.trades = g_list_append(
					market.trades,
					g_slice_dup(struct trade, &trade)
					);
				market.newest = market.trades;
				market.trades_count = 1;
			} else if (trade_equal(&trade, (struct trade *)
					       market.newest->data)) {
				goto end;
			} else {
				GList *p;
				p = g_list_insert(market.newest,
					      g_slice_dup(struct trade, &trade),
					      1);
				market.trades_count++;
			}
			break;
		}
		default:
			;
		}
	}
end:
	if (market.newest == market.trades) {
		GList *last = g_list_last(market.trades);
		market.trades = g_list_remove_link(market.trades, market.newest);
		last->next = market.newest;
		market.newest->prev = last;
		market.newest->next = NULL;
		market.earliest_updated = market.trades;
		log_data(datafile, market.trades);
		fflush(datafile);
		if (market.trades_count >= BATCH_SIZE) {
			analyze();
			discard_old_records();
		}
	} else {
		/* No data has been added */
		if (market.newest == g_list_last(market.newest)) {
			return;
		}
		log_data(datafile, market.newest->next);
		fflush(datafile);
		market.earliest_updated = market.newest->next;
		market.newest = g_list_last(market.newest);
		if (!indicators_initialized() && 
		    market.trades_count < BATCH_SIZE) {
			return;
		}
		if (indicators_initialized() &&
		    market.trades_count < BATCH_SIZE + MIN_NEW_TRADES)
			return;
		analyze();
		discard_old_records();
	}
}

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	int cap = size * nmemb;
	FILE *fp = (FILE *)userp;
	switch (my_status) {
	case registering:
		break;
	case collecting:
		fwrite(buffer, cap, 1, fp);
		break;
	default:
		break;
	}
	return cap;
}

static int collect_data(CURL *handle)
{
	int i;
	int code, ret;
	struct curl_slist *headers = NULL;
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
	FILE *fp, *datafile;
	GRegex *regex = NULL;
	GError *error = NULL;
	sprintf(temp, "Referer: https://www.avanza.se"
		"/aza/aktieroptioner/kurslistor/aktie.jsp"
		"?orderbookId=%s&grtype=intraday&sort=byID",
		orderbookId);

	for (i = 0; i < sizeof(headerlines)/sizeof(char *); i++) {
		headers = curl_slist_append(headers, headerlines[i]);		
	}
	for (i = 0; i < MAX_COOKIE_NUM && cookies[i][0]; i++)
		cookie_container[i] = cookies[i];
	cookie_container[i] = NULL;
	joined_cookies = g_strjoinv("; ", cookie_container);
	cookie = g_strjoin("", "Cookie: ", joined_cookies, NULL);
	g_free(joined_cookies);
	headers = curl_slist_append(headers, cookie);
	g_free(cookie);
	if ((code = curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers))) {
		fprintf(stderr, "Failed to set CURLOPT_HTTPHEADER. Error %d.\n", code);
		ret = code;
		goto end;
	}
	if ((code = curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_data))) {
		fprintf(stderr, "curl_easy_setopt failed with CURLOPT_WRITEFUNCTION. Error %d.\n",
			code);
		ret = code;
		goto end;
	}
	/*
https://www.avanza.se/aza/aktieroptioner/kurslistor/avslut.jsp?orderbookId=183828
->
https://www.avanza.se/aza/login/login.jsp?azaSession=true&redirectTo=https://www.avanza.se/aza/aktieroptioner/kurslistor/avslut.jsp%3Fpassword%3D2Oceans%3F%26orderbookId%3D183828%26username%3Dsinbaski&clientIp=123.86.138.148
->
https://www.avanza.se/aza/aktieroptioner/kurslistor/avslut.jsp?password=2Oceans?&orderbookId=183828&username=sinbaski
 */
	sprintf(temp,
		"https://www.avanza.se/aza/aktieroptioner/"
		"kurslistor/avslut.jsp?"
		"password=2Oceans?&orderbookId=%s&username=sinbaski",
		orderbookId);
	if ((code = curl_easy_setopt(handle, CURLOPT_URL, temp))) {
		ret = code;
		goto end;
	}
	my_status = collecting;
	regex = g_regex_new("^<TR.*><TD.*>[<]*</TD>"
			    "<TD.*>[<]*</TD>"
			    "<TD.*>([A-Z]+)</TD>"
			    "<TD.*>([0-9]{2}:[0-9]{2}:[0-9]{2})</TD>"
			    "<TD.*>([0-9]+,[0-9]{2})</TD>"
			    "<TD.*>([0-9]+)</TD></TR>$",
			    0, 0, &error);
	datafile = open_data_file();
	for (i = 0; g_atomic_int_get(&my_status) == collecting; i = 1) {
		fp = fopen("./intraday.html", "w");
		if (i == 0)
			memset(&market, 0, sizeof(market));
		if ((code = curl_easy_setopt(handle, CURLOPT_WRITEDATA, fp))) {
			fprintf(stderr, "curl_easy_setopt failed with CURLOPT_WRITEDATA. Error %d.\n",
				code);
			fclose(fp);
			ret = code;
			goto end;
		}
		curl_easy_perform(handle);
		fclose(fp);
		fp = fopen("./intraday.html", "r");
		refine_data(fp, datafile, regex);
		fclose(fp);
		sleep(DATA_UPDATE_INTERVAL);
	}
end:
	fclose(datafile);
	curl_slist_free_all(headers);
	g_list_free_full(market.trades, free_trade);
	g_regex_unref(regex);
	return ret;
}

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

static int login(CURL *handle)
{
	int ret = 0, code;
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
	const char *urls[] = {
		"https://www.avanza.se"
		"/aza/login/login.jsp",

		/* "https://www.avanza.se" */
		/* "/aza/home/home.jsp" */
	};

	for (i = 0; i < sizeof(headerlines)/sizeof(char *); i++) {
		headers = curl_slist_append(headers, headerlines[i]);		
	}
	if ((code = curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers))) {
		fprintf(stderr, "Failed to set CURLOPT_HTTPHEADER. Error %d.\n", code);
		ret = code;
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
	for (i = 0; i < sizeof(urls)/sizeof(char *); i++) {
		if ((code = curl_easy_setopt(handle, CURLOPT_URL, urls[i]))) {
			ret = code;
			goto end;
		}
		my_status = registering;
		curl_easy_perform(handle);
	}

end:
	curl_slist_free_all(headers);
	return ret;
}

int main(int argc, const char *argv[])
{
	CURLcode code;
	CURL *handle;
	int ret = 0;
	struct trade_position po;
#ifndef DEBUG
	time_t now;
#endif
	if (argc < 6) {
		fprintf(stderr, "Usage: \n"
			"%s orderbookId mode status price quantity\n", argv[0]);
		return 0;
	}
	orderbookId = argv[1];
#ifndef DEBUG
	daemonize();
	for (time(&now); localtime(&now)->tm_hour < 9; time(&now))
		sleep(DATA_UPDATE_INTERVAL);
#endif	
	memset(&po, 0, sizeof(po));
	sscanf(argv[2], "%d", (int *)&po.mode);
	sscanf(argv[3], "%d", (int *)&po.status);
	sscanf(argv[4], "%lf", &po.price);
	sscanf(argv[5], "%ld", &po.quantity);
	set_position(&po);

	memset(&cookies[0][0], 0, MAX_COOKIE_NUM * MAX_COOKIE_LEN);
	signal(SIGTERM, signal_handler);

	if ((code = curl_global_init(CURL_GLOBAL_ALL))) {
		fprintf(stderr, "curl_global_init failed "
			"with %u\n", code);
		ret = code;
		goto end1;
	}
	if (!(handle = curl_easy_init())) {
		fprintf(stderr, "curl_easy_init() failed.\n");
		ret = -1;
		goto end2;
	}
	if ((code = curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_data))) {
		fprintf(stderr, "curl_easy_setopt failed with CURLOPT_WRITEFUNCTION. Error %d.\n",
			code);
		ret = code;
		goto end3;
	}
	if ((code = curl_easy_setopt(handle, CURLOPT_WRITEDATA, NULL))) {
		fprintf(stderr, "curl_easy_setopt failed with CURLOPT_WRITEDATA. Error %d.\n",
			code);
		ret = code;
		goto end3;
	}
	if ((code = curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, write_header))) {
		fprintf(stderr, "curl_easy_setopt failed with CURLOPT_WRITEDATA. Error %d.\n",
			code);
		ret = code;
		goto end3;
	}
	if ((code = curl_easy_setopt(handle, CURLOPT_WRITEHEADER, NULL))) {
		fprintf(stderr, "curl_easy_setopt failed with CURLOPT_WRITEDATA. Error %d.\n",
			code);
		ret = code;
		goto end3;
	}

	if ((code = login(handle))) {
		ret = code;
		goto end3;
	}
	collect_data(handle);
end3:
	curl_easy_cleanup(handle);
end2:
	curl_global_cleanup();
end1:
	return ret;
}
