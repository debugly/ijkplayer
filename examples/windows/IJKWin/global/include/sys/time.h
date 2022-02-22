#ifndef _TIME_H
#define _TIME_H

#include <WinSock2.h>

#ifdef __cplusplus
extern "C"
{
#endif

long FILETIMEToUNIXTime(FILETIME const* ft,long* microseconds);
int gettimeofday(struct timeval* tv, void*dummy);

#ifdef __cplusplus
}
#endif

#endif