#ifndef _UNISTD_H
#define _UNISTD_H

#include <direct.h>
#include <io.h>
#include <process.h>

#define mkdir _mkdir
#define getpid _getpid
#define getcwd _getcwd

#ifdef __cplusplus
extern "C" {
#endif

void usleep(long long usec);

unsigned long gettid();

#ifdef __cplusplus
}
#endif

#endif /* _UNISTD_H */