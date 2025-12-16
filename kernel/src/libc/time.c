#include <time.h>
#include <lprintf.h>
#include <timer.h>
#include <stdlib.h>
#include <spinlock.h>
#include <string.h>
#include <string.h>
#include <stdio.h>
#include <rtc.h>
#include <stdlib.h>

clock_t clock (void) { return timer_get_ticks(); }

// Return current wall-clock time in seconds since Unix epoch, sourced from RTC.
time_t time(time_t *__timer) {
    uint64_t epoch = rtc_read_epoch();
    time_t t = (time_t)epoch;
    if (__timer) *__timer = t;
    return t;
}

double difftime(time_t __time1, time_t __time0)
{
    return (double)(__time1 - __time0);
}

time_t mktime(struct tm *__tp)
{
    // Simplified implementation: only handles years >= 1970 and ignores leap seconds (yes yes i know im lazy)
    if (!__tp) return (time_t)-1;
    int year = __tp->tm_year + 1900;
    if (year < 1970 || __tp->tm_mon < 0 || __tp->tm_mon > 11 || __tp->tm_mday < 1 || __tp->tm_mday > 31 ||
        __tp->tm_hour < 0 || __tp->tm_hour > 23 || __tp->tm_min < 0 || __tp->tm_min > 59 || __tp->tm_sec < 0 || __tp->tm_sec > 60) {
        return (time_t)-1;
    }
    static const int days_in_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int days = 0;
    for (int y = 1970; y < year; ++y) {
        days += 365 + (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0) ? 1 : 0);
    }
    for (int m = 0; m < __tp->tm_mon; ++m) {
        days += days_in_month[m];
        if (m == 1 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) {
            days += 1; // leap day
        }
    }
    days += (__tp->tm_mday - 1);
    time_t t = (time_t)days * 86400 + __tp->tm_hour * 3600 + __tp->tm_min * 60 + __tp->tm_sec;
    return t;
}

size_t strftime(char *restrict s, size_t maxsize,
                   const char *restrict format,
                   const struct tm *restrict tp) {
    if (!s || maxsize == 0 || !format || !tp) return 0;

    size_t len = 0;

    for (const char *p = format; *p && len < maxsize - 1; ++p) {
        if (*p == '%') {
            ++p;
            char temp[32];
            int written = 0;

            switch (*p) {
                case 'Y': written = snprintf(temp, sizeof(temp), "%04d", tp->tm_year + 1900); break;
                case 'm': written = snprintf(temp, sizeof(temp), "%02d", tp->tm_mon + 1); break;
                case 'd': written = snprintf(temp, sizeof(temp), "%02d", tp->tm_mday); break;
                case 'H': written = snprintf(temp, sizeof(temp), "%02d", tp->tm_hour); break;
                case 'M': written = snprintf(temp, sizeof(temp), "%02d", tp->tm_min); break;
                case 'S': written = snprintf(temp, sizeof(temp), "%02d", tp->tm_sec); break;
                case '%': temp[0] = '%'; temp[1] = '\0'; written = 1; break;
                default:  // Unsupported, copy literally
                    temp[0] = '%';
                    temp[1] = *p;
                    temp[2] = '\0';
                    written = 2;
                    break;
            }

            if (len + written >= maxsize) break;
            memcpy(s + len, temp, written);
            len += written;
        } else {
            if (len + 1 >= maxsize) break;
            s[len++] = *p;
        }
    }

    s[len] = '\0';
    return len;
}

char *strptime(const char *__restrict __s, const char *__restrict __fmt, struct tm *__tp)
{
    // Minimal implementation: only supports %Y-%m-%d %H:%M:%S format
    if (!__s || !__fmt || !__tp) return NULL;
    memset(__tp, 0, sizeof(struct tm));
    int year, month, day, hour, min, sec;
    if (sscanf(__s, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &min, &sec) != 6) {
        return NULL;
    }
    __tp->tm_year = year - 1900;
    __tp->tm_mon = month - 1;
    __tp->tm_mday = day;
    __tp->tm_hour = hour;
    __tp->tm_min = min;
    __tp->tm_sec = sec;
    return (char *)__s + strlen(__s); // return pointer to end of parsed string
}

char *strptime_l(const char *__restrict __s, const char *__restrict __fmt, struct tm *__tp, __locale_t __loc)
{
    (void)__loc; // currently unused
    return strptime(__s, __fmt, __tp);
}

// Timezone state
static spinlock_t tz_lock;
static int tz_lock_inited;
char *__tzname[2] = { (char*)"UTC", (char*)"UTC" };
int __daylight = 0; // non-zero if DST is ever used
long int __timezone = 0; // seconds west of UTC; UTC = 0
char *tzname[2];
int daylight;
long int timezone;

// Basic POSIX TZ parser: "STD[+/-]hh[:mm[:ss]][DST[+/-]hh[:mm[:ss]]]"
// e.g., "UTC0", "EST5EDT", "CST6CDT", "PST8PDT". Offsets are hours west of UTC.
static int parse_posix_tz(const char* s, long* tz_off_sec, int* has_dst, long* dst_off_sec,
                          const char** std_abbr, const char** dst_abbr) {
    if (!s || !*s) return -1;
    static char std_buf[8], dst_buf[8];
    const char* p = s;
    // Read STD abbr letters
    int i=0; while (*p && ((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z'))) { if (i<7) std_buf[i++]=*p; p++; }
    std_buf[i]='\0'; *std_abbr = std_buf;
    // Parse offset (hours west of UTC)
    int sign = 1; if (*p=='-') { sign = -1; p++; } else if (*p=='+') { sign = 1; p++; }
    long hh=0, mm=0, ss=0; while (*p>='0'&&*p<='9') { hh = hh*10 + (*p++ - '0'); }
    if (*p==':') { p++; while (*p>='0'&&*p<='9') { mm = mm*10 + (*p++ - '0'); }
        if (*p==':') { p++; while (*p>='0'&&*p<='9') { ss = ss*10 + (*p++ - '0'); } } }
    long off = sign * (hh*3600 + mm*60 + ss);
    *tz_off_sec = off;

    // Optionally DST abbr and offset
    i=0; if (*p) { while (*p && ((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z'))) { if (i<7) dst_buf[i++]=*p; p++; } }
    dst_buf[i]='\0';
    if (i>0) { *has_dst = 1; *dst_abbr = dst_buf; }
    else { *has_dst = 0; *dst_abbr = NULL; }

    if (*has_dst) {
        // Optional DST offset
        long dhh=0,dmm=0,dss=0; int dsign=1;
        if (*p=='-'||*p=='+') { dsign = (*p=='-')?-1:1; p++; }
        if (*p>='0'&&*p<='9') { while (*p>='0'&&*p<='9') { dhh = dhh*10 + (*p++ - '0'); }
            if (*p==':') { p++; while (*p>='0'&&*p<='9') { dmm = dmm*10 + (*p++ - '0'); }
                if (*p==':') { p++; while (*p>='0'&&*p<='9') { dss = dss*10 + (*p++ - '0'); } } }
        }
        if (dhh == 0 && dmm == 0 && dss == 0) {
            *dst_off_sec = off - 3600; // Default DST offset is STD - 1 hour
        } else {
            *dst_off_sec = dsign * (dhh*3600 + dmm*60 + dss);
        }
    } else {
        *dst_off_sec = 0;
    }
    // Skip any remaining DST rules (e.g., ,M3.2.0/2,M11.1.0/2)
    while (*p && *p != '\0') p++;
    return 0;
}

static void apply_tz_defaults(void) {
    tzname[0] = __tzname[0]; tzname[1] = __tzname[1];
    daylight = __daylight; timezone = __timezone;
}

void tzset(void) {
    if (!tz_lock_inited) { spinlock_init(&tz_lock); tz_lock_inited = 1; }
    const char* tz = getenv("TZ");
    if (!tz || !*tz) { // default UTC
        spin_lock(&tz_lock);
        __tzname[0] = (char*)"UTC"; __tzname[1] = (char*)"UTC";
        __timezone = 0; __daylight = 0; apply_tz_defaults();
        spin_unlock(&tz_lock);
        return;
    }
    // For now only POSIX-style supported (IANA would need zoneinfo DB)
    long off=0, dst_off=0; int has_dst=0; const char *std_abbr=NULL,*dst_abbr=NULL;
    spin_lock(&tz_lock);
    if (parse_posix_tz(tz, &off, &has_dst, &dst_off, &std_abbr, &dst_abbr) == 0) {
        __tzname[0] = (char*)std_abbr;
        __tzname[1] = (char*)(has_dst ? dst_abbr : std_abbr);
        __timezone = off;
        __daylight = has_dst;
        apply_tz_defaults();
    } else {
        __tzname[0] = (char*)"UTC"; __tzname[1] = (char*)"UTC";
        __timezone = 0; __daylight = 0; apply_tz_defaults();
    }
    spin_unlock(&tz_lock);
}

static void epoch_to_tm_utc(time_t t, struct tm* out) {
    static const int days_in_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int64_t sec = (int64_t)t;
    int y = 1970;
    while (1) {
        int leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
        int diy = 365 + leap;
        if (sec < (int64_t)diy * 86400) {
            break;
        }
        sec -= (int64_t)diy * 86400;
        y++;
    }
    out->tm_year = y - 1900;
    out->tm_yday = (int)(sec / 86400);
    sec %= 86400;
    out->tm_hour = (int)(sec / 3600); sec %= 3600;
    out->tm_min  = (int)(sec / 60);   out->tm_sec = (int)(sec % 60);
    int month = 0; int yy = y;
    int yday = out->tm_yday;
    while (month < 12) {
        int dim = days_in_month[month] + (month==1 && (yy % 4 == 0 && (yy % 100 != 0 || yy % 400 == 0)));
        if (yday < dim) {
            break;
        }
        yday -= dim;
        month++;
    }
    out->tm_mon = month; out->tm_mday = yday + 1;
    out->tm_wday = (4 + (t / 86400)) % 7; // 1970-01-01 was Thursday
    out->tm_isdst = 0; out->tm_gmtoff = 0; out->tm_zone = "UTC";
}

static void tm_apply_tz(struct tm* t) {
    // Apply offset with basic DST check (approximating US rules: DST March-Oct)
    long off = __timezone; // seconds west of UTC
    if (__daylight && t->tm_mon >= 2 && t->tm_mon <= 10) { // March (2) to October (10)
        off = __timezone - 3600; // Use DST offset (assuming 1 hour difference)
        t->tm_isdst = 1;
        t->tm_zone = __tzname[1];
    } else {
        t->tm_isdst = 0;
        t->tm_zone = __tzname[0];
    }
    int east = -off; // seconds east
    // Convert UTC->local by adding east offset
    time_t base = mktime(t); // t as UTC
    time_t local = base + east;
    epoch_to_tm_utc(local, t);
    t->tm_gmtoff = east;
}

struct tm *gmtime(const time_t *__timer)
{
    if (!__timer) return NULL;
    time_t t = *__timer;
    struct tm *result = (struct tm *)malloc(sizeof(struct tm));
    if (!result) return NULL;
    memset(result, 0, sizeof(struct tm));
    epoch_to_tm_utc(t, result);
    return result;
}

struct tm *gmtime_r(const time_t *__restrict __timer, struct tm *__restrict __tp) {
    if (!__timer || !__tp) return NULL;
    epoch_to_tm_utc(*__timer, __tp);
    return __tp;
}

struct tm *localtime(const time_t *__timer) {
    if (!__timer) return NULL;
    tzset();
    struct tm* r = gmtime(__timer);
    if (!r) return NULL;
    tm_apply_tz(r);
    return r;
}

struct tm *localtime_r(const time_t *__restrict __timer, struct tm *__restrict __tp) {
    if (!__timer || !__tp) return NULL;
    tzset();
    epoch_to_tm_utc(*__timer, __tp);
    tm_apply_tz(__tp);
    return __tp;
}

char *asctime(const struct tm *__tp) {
    static char buf[26];
    if (!__tp) return NULL;
    // Day and month names placeholders
    static const char* wday[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char* mon[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    snprintf(buf, sizeof(buf), "%s %s %02d %02d:%02d:%02d %04d\n",
             wday[__tp->tm_wday%7], mon[__tp->tm_mon%12], __tp->tm_mday,
             __tp->tm_hour, __tp->tm_min, __tp->tm_sec, __tp->tm_year+1900);
    return buf;
}

char *ctime(const time_t *__timer) {
    static char buf[26];
    struct tm t;
    if (!localtime_r(__timer, &t)) return NULL;
    const char* s = asctime(&t);
    if (!s) return NULL;
    strncpy(buf, s, sizeof(buf)); buf[sizeof(buf)-1]='\0';
    return buf;
}

char *asctime_r(const struct tm *__restrict __tp, char *__restrict __buf) {
    if (!__tp || !__buf) return NULL;
    // Day and month names placeholders
    static const char* wday[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char* mon[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    snprintf(__buf, 26, "%s %s %02d %02d:%02d:%02d %04d\n",
             wday[__tp->tm_wday%7], mon[__tp->tm_mon%12], __tp->tm_mday,
             __tp->tm_hour, __tp->tm_min, __tp->tm_sec, __tp->tm_year+1900);
    return __buf;
}

char *ctime_r(const time_t *__restrict __timer, char *__restrict __buf) {
    struct tm t;
    if (!localtime_r(__timer, &t)) return NULL;
    return asctime_r(&t, __buf);
}

__attribute__((constructor)) static void __time_ctor(void) { spinlock_init(&tz_lock); }


#ifdef __USE_POSIX199309
int clock_gettime(clockid_t clk_id, struct timespec* tp) {
    if (!tp) return -1;
    switch (clk_id) {
        case 0 /* CLOCK_REALTIME */:
        {
            uint64_t seconds = rtc_read_epoch();
            tp->tv_sec = (time_t)seconds;
            // We don’t have sub-second from RTC; approximate via ticks
            uint32_t hz = timer_hz();
            clock_t ticks = timer_get_ticks();
            tp->tv_nsec = (long)((ticks % hz) * (1000000000ULL / (hz ? hz : 1000)));
            return 0;
        }
        default: // Treat others as monotonic based on ticks
        {
            uint32_t hz = timer_hz();
            clock_t ticks = timer_get_ticks();
            tp->tv_sec = (time_t)(ticks / (hz ? hz : 1000));
            tp->tv_nsec = (long)((ticks % (hz ? hz : 1000)) * (1000000000ULL / (hz ? hz : 1000)));
            return 0;
        }
    }
}

int clock_getres(clockid_t clk_id, struct timespec* res) {
    (void)clk_id;
    if (!res) return -1;
    uint32_t hz = timer_hz();
    res->tv_sec = 0;
    res->tv_nsec = (long)(1000000000ULL / (hz ? hz : 1000));
    return 0;
}
#endif

#ifdef __USE_ISOC11
int timespec_get(struct timespec* ts, int base) {
    if (!ts) return 0;
    if (base != TIME_UTC) return 0;
    // Real time based on RTC + tick fraction
    struct timespec tp; (void)clock_gettime(0, &tp);
    *ts = tp;
    return TIME_UTC;
}
#endif
