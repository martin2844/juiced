#pragma once
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <strings.h>
#include <sys/time.h>
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _wcsicmp wcscasecmp
#define _wcsnicmp wcsncasecmp
#define _strdup strdup
inline int fopen_s(FILE **f, const char *p, const char *m) {
    *f = fopen(p, m);
    return *f ? 0 : errno;
}
inline int localtime_s(tm *t, const time_t *v) {
    return localtime_r(v, t) ? 0 : errno;
}
inline int gmtime_s(tm *t, const time_t *v) {
    return gmtime_r(v, t) ? 0 : errno;
}
using __time64_t = time_t;
inline time_t _time64(time_t *t) {
    return time(t);
}
struct __timeb64 {
    time_t time;
    unsigned short millitm;
    short timezone, dstflag;
};
inline void _ftime64_s(__timeb64 *t) {
    timeval v{};
    gettimeofday(&v, nullptr);
    t->time = v.tv_sec;
    t->millitm = v.tv_usec / 1000;
}

#define _atoi64 atoll
#define fscanf_s fscanf
#define sscanf_s sscanf
#include <cfloat>
#include <sys/stat.h>
#define _stat64 stat
#define _fstat64 fstat
#define _fileno fileno

#define __stdcall
