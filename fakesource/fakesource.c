#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include "trade.h"

#define DATA_XCHG_FILE "./intraday.html"
#define PI 3.1416
#define BATCH_SIZE 15

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

int main (int argc, char *argv[])
{
	char message[10];
	time_t now;
	FILE *req, *resp;
	FILE  *datafile = NULL;

	if (argc == 2) {
		datafile = fopen(argv[1], "r");
	}
	time(&now);
	srand(now);
	int done = 1;
	do {
		FILE *fp;
		int i;
		long n = 0;
		struct tm *timep;
		req = fopen("./req-fifo", "r");
		fscanf(req, "%s", message);
		fclose(req);

		if (strncmp(message, "get", sizeof(message)))
			break;
		fp = fopen(DATA_XCHG_FILE, "w");
		fprintf(fp, "Avslut Company\n");
		fprintf(fp, "Antal</TD></TR>\n");

		time(&now);
		now += n++ * 60;

		for (i = 0; i < BATCH_SIZE; i++) {
			struct trade trade;
			int l, m;
			now += BATCH_SIZE - i;
			timep = localtime(&now);
			if (!datafile) {
				done = get_fake_trade(now, &trade);
				sprintf(trade.time, "%02d:%02d:%02d",
					timep->tm_hour, timep->tm_min,
					timep->tm_sec);
				sprintf(trade.market, "FAKE");
			} else if ((done = get_real_trade(&trade, datafile)))
				break;
			l = (int)floor(trade.price);
			m = (int)((trade.price - l) * 100);
			fprintf(fp, "<TR><TD>ABC</TD><TD>CBA</TD><TD>%s</TD>",
				trade.market);
			fprintf(fp, "<TD>%s</TD>", trade.time);
			fprintf(fp, "<TD>%d,%02d</TD>", l, m);
			fprintf(fp, "<TD>%ld</TD></TR>\n", trade.quantity);
		}
		fprintf(fp, "</TABLE>\n");
		fclose(fp);

		resp = fopen("./resp-fifo", "w");
		fprintf(resp, "done");
		fclose(resp);
	} while(!done);
	return 0;
}
