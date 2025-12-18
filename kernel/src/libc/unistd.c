// POSIX-like sleep functions backed by timer ticks; busy-wait until scheduler exists.
#include <unistd.h>
#include <time.h>
#include <timer.h>
#include <sched.h>

static int nanosleep_impl(const struct timespec* req, struct timespec* rem) {
	if (!req || req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= 1000000000L) return -1;
	uint64_t hz = timer_hz();
	if (!hz) hz = 1000; // fallback assumption
	uint64_t target_ns = (uint64_t)req->tv_sec * 1000000000ULL + (uint64_t)req->tv_nsec;
	uint64_t tick_ns = 1000000000ULL / hz;
	if (tick_ns == 0) tick_ns = 1;
	uint64_t target_ticks = (target_ns + tick_ns - 1ULL) / tick_ns;
	if (target_ticks == 0 && target_ns > 0) target_ticks = 1;
	if (scheduler_is_started()) {
		task_sleep_ticks(target_ticks);
	} else {
		uint64_t start = (uint64_t)timer_get_ticks();
		while (((uint64_t)timer_get_ticks() - start) < target_ticks) {
			__asm__ __volatile__("pause");
		}
	}
	if (rem) { rem->tv_sec = 0; rem->tv_nsec = 0; }
	return 0;
}

unsigned int sleep(unsigned int seconds) {
	struct timespec ts = { (time_t)seconds, 0 };
	nanosleep_impl(&ts, NULL);
	return 0;
}

int usleep(unsigned int usec) {
	struct timespec ts = { (time_t)(usec / 1000000U), (long)((usec % 1000000U) * 1000L) };
	return nanosleep_impl(&ts, NULL);
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
	return nanosleep_impl(req, rem);
}
