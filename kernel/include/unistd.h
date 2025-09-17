#ifndef UNISTD_H
#define UNISTD_H

#include <time.h>

unsigned int sleep(unsigned int seconds);
int usleep(unsigned int usec);
int nanosleep(const struct timespec* req, struct timespec* rem);

#endif /* UNISTD_H */