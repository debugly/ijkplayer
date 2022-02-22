#include "MemInfo.h"
#include "Counter.h"
#include "Util.h"

MemInfo* MemInfo_New()
{
	MemInfo* mem = NULL;
	mem = xMalloc(sizeof(MemInfo));
	if (NULL == mem){
		return NULL;
	}
	xMemSet(mem, 0 , sizeof(MemInfo));
	return mem;
}

int MemInfo_Init(MemInfo* mem)
{
	if (NULL == mem){
		return -1;
	}
	xMemSet(mem, 0 ,sizeof(MemInfo));
	return 0;
}

int MemInfo_Read(MemInfo *mem)
{
	MEMORYSTATUSEX memStat;
	PDH_HQUERY query;

	memStat.dwLength = sizeof(memStat);
	if (GlobalMemoryStatusEx(&memStat) == 0){
		return FALSE;
	}
	mem->mem_total = memStat.ullTotalPhys;
	mem->mem_free = memStat.ullAvailPhys;
	mem->swap_total = memStat.ullTotalPageFile;
	mem->swap_free = memStat.ullAvailPageFile;
	if (PdhOpenQuery(NULL, 0, &query) == ERROR_SUCCESS) {
		PDH_HCOUNTER cache, pageIn, pageOut;
		if (AddCounterToQuery(MEM_COUNTER_OBJECT, NULL, MEM_COUNTER_CACHE, &query, &cache) == ERROR_SUCCESS &&
			AddCounterToQuery(MEM_COUNTER_OBJECT, NULL, MEM_COUNTER_PAGE_IN, &query, &pageIn) == ERROR_SUCCESS &&
			AddCounterToQuery(MEM_COUNTER_OBJECT, NULL, MEM_COUNTER_CACHE, &query, &pageOut) == ERROR_SUCCESS &&
			PdhCollectQueryData(query) == ERROR_SUCCESS) {
			mem->mem_cached= GetRawCounterValue(&cache);
			mem->page_in = (unsigned int)GetRawCounterValue(&pageIn);
			mem->page_out = (unsigned int)GetRawCounterValue(&pageOut);
		}
		PdhCloseQuery(query);
	}

	//There are no obvious Windows equivalents
    mem->mem_shared = UNKNOWN_COUNTER_64;
	mem->mem_buffers = UNKNOWN_COUNTER_64;
	//using the definition that swapping is when all the memory associated with a
	//process is moved in/out of RAM
	mem->swap_in = UNKNOWN_COUNTER;
	mem->swap_out = UNKNOWN_COUNTER;
	return TRUE;
}
