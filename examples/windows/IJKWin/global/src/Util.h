#ifndef UTIL_H
#define UTIL_H

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define MAGIC_REMAINDER_MASK 0x00000003

#ifdef __cplusplus
extern "C"
{
#endif

void *xMalloc(unsigned int size);
void* xRealloc(void* pSrc, unsigned int size);
void *xCalloc(unsigned int nSize, unsigned int nCount);
void xFree(void* Ptr);

void *xMemCpy(void *pDst,const void *pSrc,unsigned int Num);
int xMemCmp(const void *pSrc, const void *pTar, unsigned int n);
void *xMemSet(void* pDst, char c, unsigned int len);
void *xMemMove(void *pDst, const void *pSrc, unsigned int size);

int xStrCmp(char *strSrc, char *strDst);
char* xStrNCpy(char* strDst, char* strSrc, unsigned int size);
int xStrNCmp(void *strSrc, void *strDst, unsigned int n);

#ifdef __cplusplus
}
#endif

#endif
