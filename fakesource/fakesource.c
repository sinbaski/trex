#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <mysql.h>
#include "trade.h"

#define DATA_XCHG_FILE "./intraday.html"
#define PI 3.1416
#define BATCH_SIZE 15
#define WORKDIR "/home/xxie/work/avanza/data_extract/intraday"

MYSQL *mysqldb;
char tbl_name[64];
char date[11];
char orderbookId[7];

int get_fake_trade(double t, struct trade *trade)
{
	static const double A0 = 80;

	static const double A1 = 2;
	static const double omg1 = 2 * PI / (3600 * 8.5);

	static const double A2 = 1.4;
	static const double omg2 = 2 * PI / (3600 * 2);

	static const double A3 = 0.4;
	static const double omg3 = 2 * PI / (60 * 10);

	trade->price = A0 + A1 * sinf(omg1 * t) + A2 * sinf(omg2 * t)
		+ A3 * sinf(omg3 * t) + (double)rand() / RAND_MAX * 0.55;
	trade->quantity = rand() % 1000;
	return 0;
}

int get_real_trade(struct trade *trade, FILE *datafile)
{
	static long offset = BATCH_SIZE;
	static long n = 2;

	fseek(datafile, --offset * DATA_ROW_WIDTH, SEEK_SET);
	if (offset % BATCH_SIZE == 0) {
		offset = BATCH_SIZE * n++;
		if (offset >= fnum_of_line(datafile))
			return 1;
	}
	fscanf(datafile, "%s\t%s\t%lf\t%ld",
	       trade->market, trade->time, &trade->price,
	       &trade->quantity);
	return 0;
}

static void output_record(FILE *fp, const struct trade *trade)
{
	int l, m;
	char *p;
#define rowsize 100
	static int n = 0;
	static char buffer[BATCH_SIZE * rowsize];
	if (n == 0)
		memset(buffer, rowsize * BATCH_SIZE, 0);
	p = buffer + n * rowsize;
	l = (int)floor(trade->price);
	m = (int)((trade->price - l) * 100);
	sprintf(p, "<TR><TD>ABC</TD><TD>CBA</TD><TD>%s</TD>"
		"<TD>%s</TD><TD>%d,%02d</TD><TD>%ld</TD></TR>\n",
		trade->market, trade->time, l, m, trade->quantity);
	n++;

	if (n == BATCH_SIZE) {
		int i;
		for (i = BATCH_SIZE; i > 0;)
			fprintf(fp, "%s", buffer + --i * rowsize);
		n = 0;
	}
}

int main (int argc, char *argv[])
{
	char message[10];
	time_t now;
	FILE *req, *resp;
	MYSQL_RES *res;
	MYSQL_ROW row;
	char fname[50], buffer[128];
	int done = 0;

	if (argc != 3) {
		printf("Usage: %s tbl_name date\n", argv[0]);
		return 0;
	}
	chdir(WORKDIR);
	strcpy(tbl_name, argv[1]);
	strcpy(date, argv[2]);
	time(&now);
	srand(now);
	mysqldb = mysql_init(NULL);
	if (!mysql_real_connect(mysqldb, "localhost",
				"sinbaski", "q1w2e3r4",
				"avanza", 0, NULL, 0)) {
		fprintf(stderr, "%s\n", mysql_error(mysqldb));
		return -1;
	}
	sprintf(buffer, "select dataid from company where name=\"%s\"",
		tbl_name);
	mysql_query(mysqldb, buffer);
	res = mysql_store_result(mysqldb);
	row = mysql_fetch_row(res);
	strcpy(orderbookId, row[0]);
	mysql_free_result(res);	
	if (strcmp(tbl_name, "fake") != 0) {
		sprintf(buffer, 
			"select time(tid), price, volume from %s where "
			"tid like \"%s %%\" order by tid asc;",
			tbl_name, date);
		mysql_query(mysqldb, buffer);
		res = mysql_use_result(mysqldb);
	}

	sprintf(fname, "intraday-%s.html", orderbookId);

	done = 0;
	do {
		FILE *fp;
		int i;
		long n = 0;
		struct tm *timep;
		struct trade trade;

		req = fopen("./req-fifo", "r");
		fscanf(req, "%s", message);
		fclose(req);

		if (strncmp(message, "get", sizeof(message)))
			break;

		fp = fopen(fname, "w");
		fprintf(fp, "Avslut Company\n");
		fprintf(fp, "Antal</TD></TR>\n");

		time(&now);
		now += n++ * 60;

		if (strcmp(tbl_name, "fake") == 0) {
			for (i = 0; i < BATCH_SIZE && !done; i++) {
				now += BATCH_SIZE - i;
				timep = localtime(&now);
				done = get_fake_trade(now, &trade);
				sprintf(trade.time, "%02d:%02d:%02d",
					timep->tm_hour, timep->tm_min,
					timep->tm_sec);
				sprintf(trade.market, "FAKE");
				output_record(fp, &trade);
			}
		} else {
			for (i = 0; i < BATCH_SIZE && !done; i++) {			
				row = mysql_fetch_row(res);
				if (!row) {
					done = 1;
					continue;
				}
				sprintf(trade.market, "FAKE");
				strncpy(trade.time, row[0],
					sizeof(trade.time));
				sscanf(row[1], "%lf", &trade.price);
				sscanf(row[2], "%ld", &trade.quantity);
				output_record(fp, &trade);
			}
		}
		fprintf(fp, "</TABLE>\n");
		fclose(fp);

		resp = fopen("./resp-fifo", "w");
		if (!done)
			fprintf(resp, "ready\n");
		else
			fprintf(resp, "done\n");
		fclose(resp);
	} while(!done);
	if (strcmp(tbl_name, "fake") != 0) {
		mysql_free_result(res);
	}
	mysql_close(mysqldb);
	return 0;
}
