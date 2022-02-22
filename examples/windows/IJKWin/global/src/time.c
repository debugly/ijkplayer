#include <sys/time.h>

long FILETIMEToUNIXTime(FILETIME const* ft,long* microseconds)
{
    LONGLONG    i;

    i = ft->dwHighDateTime;
    i <<= 32;
    i |= ft->dwLowDateTime;

    i -= 116444736000000000L;
    if(NULL != microseconds)
    {
        *microseconds = (long)((i % 10000000) / 10);
    }
    i /= 10000000;
    return (long)i;
}


int gettimeofday(struct timeval*  tv,void*dummy)
{
	SYSTEMTIME  st;
	FILETIME    ft;

	((void)dummy);
	GetSystemTime(&st);
	(void)SystemTimeToFileTime(&st, &ft);
	tv->tv_sec = FILETIMEToUNIXTime(&ft, &tv->tv_usec);

	return 0;
}