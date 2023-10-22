#ifndef __UTILS_H__
#define __UTILS_H__

#include <sys/time.h>

void printstats(struct timeval *tstart, struct timeval *tstop, unsigned long bytes);

#endif /* __UTILS_H__ */
