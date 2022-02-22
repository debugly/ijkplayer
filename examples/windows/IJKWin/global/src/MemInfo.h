#ifndef _MEMINFO_H_
#define _MEMINFO_H_

typedef struct MEMINFO_T{
	unsigned __int64 mem_total;    /* total bytes */
	unsigned __int64 mem_free;     /* free bytes */
	unsigned __int64 mem_shared;   /* shared bytes */
	unsigned __int64 mem_buffers;  /* buffers bytes */
	unsigned __int64 mem_cached;   /* cached bytes */
	unsigned __int64 swap_total;   /* swap total bytes */
	unsigned __int64 swap_free;    /* swap free bytes */
	unsigned int page_in;      /* page in count */
	unsigned int page_out;     /* page out count */
	unsigned int swap_in;      /* swap in count */
	unsigned int swap_out;     /* swap out count */
}MemInfo;

#ifdef __cplusplus
extern "C"
{
#endif

	MemInfo* MemInfo_New();
	int MemInfo_Init(MemInfo*);
	int MemInfo_Read(MemInfo*);

#ifdef __cplusplus
}
#endif

#endif


