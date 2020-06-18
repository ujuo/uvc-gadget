#include <stdio.h>
#include <sys/time.h>

#include <capis.h>

int debug_level=2;
FILE *dfp;

static unsigned int start_tv;

void timer_init()
{
	struct timeval tv;

	gettimeofday(&tv,NULL);
	start_tv = (tv.tv_sec*1000 + tv.tv_usec/1000);
}

void cur_time(FILE *fp)
{
	struct timeval tv;
	unsigned int t;
	int m,s,m1000;

	gettimeofday(&tv,NULL);
	/*t = (tv.tv_sec*1000 + tv.tv_usec/1000) - start_tv;
	m1000 = t % 1000;
	s = t / 1000;
	m = s / 60;
	s = s - m * 60; */
	fflush(fp);
	fprintf(fp, "%.6d.%.6d-", tv.tv_sec, tv.tv_usec);
}
