#ifndef _UTILS_H_
#define _UTILS_H_Z

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <io.h>
#include <time.h>
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h> 

#define lseek _lseek
#define close _close
#define open _open
#define read _read
#define write _write
#define unlink _unlink
#define access _access
#define strdup _strdup

#define F_OK 0 
#define W_OK 2
#define R_OK 4 

#if !defined(snprintf)
#define snprintf  _snprintf
#define PRId8     "hhd"
#define PRId16    "hd"
#define PRId32    "ld"
#define PRId64    "lld"
#define PRIx64	  "llx"
#endif

#define uint32_t in_addr_t

#define S_IFLNK    0120000 /* Symbolic link */
#define S_ISLNK(x) (((x) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(x) 0
#define S_IRGRP 0
#define S_IWGRP 0
#define S_IXGRP 0
#define S_ISGID 0
#define S_IROTH 0
#define S_IXOTH 0

#define	S_IRWXU	0000700			/* RWX mask for owner */
#define	S_IRUSR	0000400			/* R for owner */
#define	S_IWUSR	0000200			/* W for owner */
#define	S_IXUSR	0000100			/* X for owner */

#define SIGHUP 1
#define SIGQUIT 3
#define SIGKILL 9
#define SIGPIPE 13
#define SIGALRM 14
#define SIGCHLD 17

#define SIGUSR2	300

#define	_SC_NPROCESSORS_CONF		57
#define	_SC_NPROCESSORS_ONLN		58

#ifndef ST_UTIME_NO_TIMEOUT
#define ST_UTIME_NO_TIMEOUT -1
#endif

//#define ECONNRESET            232
//#define ETIMEDOUT             238
//#ifndef ETIME
//#define ETIME ETIMEDOUT
//#endif

#ifdef __cplusplus
extern "C"
{
#endif

#define NUM_ELEMENTS(ar) (sizeof(ar) / sizeof(ar[0]))

int Errno_From_Win32(unsigned long w32Err);
void* Win32_Handle_From_File(int fd);

void initsocket(void);
long sysconf(int name);

#ifdef __cplusplus
}
#endif


#endif