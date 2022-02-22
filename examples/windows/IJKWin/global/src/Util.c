#include "Util.h"

void *xMalloc(unsigned int size)
{
	void *PTR = NULL;

	PTR = HeapAlloc(GetProcessHeap(), 0, size);
	if (PTR == NULL) {
		return NULL;
	}
	return PTR;
}

void* xRealloc(void* pSrc, unsigned int size)
{
	void *PTR = NULL;

	if (NULL == pSrc && !size){
		PTR = HeapAlloc(GetProcessHeap(), 0, size);
		if (PTR == NULL) {
			return NULL;
		}
		return PTR;
	}
	if (!size){
		return NULL;
	}
	PTR = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, pSrc, size);
	if(NULL == PTR){
		return NULL;
	}
	return PTR;
}

void *xCalloc(unsigned int nSize, unsigned int nCount)
{
	unsigned int size = 0;
	void* PTR = NULL;

	size = nSize * nCount;
	if (!size){
		return NULL;
	}
	PTR = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
	if (NULL == PTR){
		return NULL;
	}

	return PTR;
}

void xFree(void* PTR)
{
	(void)HeapFree(GetProcessHeap(), 0, PTR);
}

void *xMemCpy(void *pDst,const void *pSrc,unsigned int Num)
{
	int dwordNum = Num >> 2;
	int slice = Num & MAGIC_REMAINDER_MASK;
	int *tmpSrc = NULL;
	int *tmpDst = NULL;

	if((NULL == pDst) ||(NULL == pSrc) || !Num){
		return NULL;
	}
	tmpSrc = (int*)pSrc;
	tmpDst = pDst;

	while(dwordNum--)*tmpDst++ = *tmpSrc++;
	while(slice--)*((char *)tmpDst++) =*((char *)tmpSrc++);
	return pDst;  
}


int xMemCmp(const void *pSrc, const void *pTar, unsigned int n)
{
	const unsigned char *src = pSrc;
	const unsigned char *tar = pTar;
	int ret = 0;

	if (NULL == src || NULL == tar){
		return -1;
	}

	while (n--) {
		ret = (int)*src++ - (int)*tar++;
		if (ret)
			break;
	}

	return ret;
}


void *xMemSet(void* pDst, char c, unsigned int len)
{
	char *p = NULL;
	for( p = pDst; len; --len ) {
		*p++ = c;
	}
	return pDst;
}


void *xMemMove(void *pDst, const void *pSrc, unsigned int size)
{
	const char *p = pSrc;
	char *q = pDst;

	if (q < p) {
		while (size--) {
			*q++ = *p++;
		}
	} else {
		p += size;
		q += size;
		while (size--) {
			*--q = *--p;
		}
	}
	return pDst;
}


int xStrCmp(char *strSrc, char *strDst)
{
	unsigned char *pSrc = (unsigned char *)strSrc;
	unsigned char *pDst = (unsigned char *)strDst;
	unsigned char ch;
	int result = 0;

	do{
		result = (int)(ch = *pSrc++) - (int)*pDst++;
		if (result || !ch)
			break;
	}
	while(TRUE);
	return result;
}

char* xStrNCpy(char* strDst, char* strSrc, unsigned int size)
{
	char *dst = strDst;
	char *src = strSrc;
	char ch;

	if (!size){
		return NULL;
	}

	while (size) {
		size--;
		*dst++ = ch = *src++;
		if (!ch)
			break;
	}

	xMemSet(dst, 0, size);
	return dst;
}

int xStrNCmp(void *strSrc, void *strDst, unsigned int n)
{
	unsigned char *src = (unsigned char *)strSrc;
	unsigned char *dst = (unsigned char *)strDst;
	unsigned char ch;
	int d = 0;

	while (n--) {
		d = (int)(ch = *src++) - (int)*dst++;
		if (d || !ch)
			break;
	}

	return d;
}
