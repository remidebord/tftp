#include <stdio.h>

#include "utils.h"

void printstats(struct timeval *tstart, struct timeval *tstop, unsigned long bytes)
{
	double delta;

	delta = (tstop->tv_sec + (tstop->tv_usec / 100000.0)) - (tstart->tv_sec + (tstart->tv_usec / 100000.0));

	printf("%lu bytes in %.1f seconds", bytes, delta);
	printf(" [%.0f bit/s]", (bytes * 8.) / delta);
	putchar('\n');
}
