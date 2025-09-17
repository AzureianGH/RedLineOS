// POSIX-like sleep functions backed by kernel scheduler
#include <unistd.h>
#include <sched.h>
#include <time.h>

unsigned int sleep(unsigned int seconds) {
	sched_sleep_ms((uint64_t)seconds * 1000ULL);
	return 0;
}

int usleep(unsigned int usec) {
	uint64_t ms = (uint64_t)(usec / 1000u);
	if (ms == 0 && usec) ms = 1; // minimum 1ms
	sched_sleep_ms(ms);
	return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
	if (!req) return -1;
	uint64_t ms = (uint64_t)req->tv_sec * 1000ULL + (uint64_t)(req->tv_nsec / 1000000ULL);
	if (ms == 0 && (req->tv_sec || req->tv_nsec)) ms = 1;
	sched_sleep_ms(ms);
	if (rem) { rem->tv_sec = 0; rem->tv_nsec = 0; }
	return 0;
}
