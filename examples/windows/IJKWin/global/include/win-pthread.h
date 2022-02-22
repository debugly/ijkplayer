#ifndef _WIN_PTHREAD_H_
#define _WIN_PTHREAD_H_

#include <pthread.h>
#ifdef __cplusplus
extern "C"
{
#endif

int pthread_setname_np (pthread_t thr, const char * name);

#ifdef __cplusplus
}
#endif


#endif