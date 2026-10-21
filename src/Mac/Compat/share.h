#pragma once
#include <cstdio>
#define _SH_DENYNO 0
inline FILE *_fsopen(const char *p, const char *m, int) {
    return fopen(p, m);
}
