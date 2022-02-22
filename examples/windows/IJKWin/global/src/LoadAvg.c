#include "LoadAvg.h"
#include "CpuInfo.h"
#include "Util.h"

LoadAvg* LoadAvg_New()
{
	LoadAvg* la = NULL;
	la = xMalloc(sizeof(LoadAvg));
	if (NULL == la){
		return NULL;
	}
	la->load_1 = 0.0f;
	la->load_5 = 0.0f;
	la->load_15 = 0.0f;
	la->procQuery = NULL;
	la->procTimeCounter = NULL;
	return la;
}

int LoadAvg_Init(LoadAvg* la)
{
	if (NULL == la){
		return -1;
	}
	la->load_1 = 0.0f;
	la->load_5 = 0.0f;
	la->load_15 = 0.0f;
	la->procQuery = NULL;
	la->procTimeCounter = NULL;
	return 0;
}

int LoadAvg_CalcLoad(LoadAvg* la)
{
	unsigned int queuelen = 0;
	double cpuload, load;
	PDH_HQUERY query;
	PDH_HCOUNTER counter;
	if (MakeSingleCounterQuery(SYS_COUNTER_OBJECT, NULL, SYS_COUNTER_PROC_QLEN, &query, &counter) == ERROR_SUCCESS &&
		PdhCollectQueryData(query) == ERROR_SUCCESS) {
			queuelen = (unsigned int)GetRawCounterValue(&counter);
	}
	if (query) {
		PdhCloseQuery(query);
	}
	cpuload = LoadAvg_GetCpuLoad(la);
	if (queuelen > 2) {
		load = cpuload + queuelen - 2;
	} else {
		load = cpuload;
	}
	la->load_1 = la->load_1 * 0.9200 + load * 0.0800;
	la->load_5 = la->load_5 * 0.9835 + load * 0.0165;
	la->load_15 = la->load_15 * 0.9945 + load * 0.0055;
	return 0;
}

double LoadAvg_GetCpuLoad(LoadAvg* la)
{
	PDH_STATUS status;
	DWORD dwType;
	PDH_FMT_COUNTERVALUE value;

	if (la->procQuery == NULL) {
		status = MakeSingleCounterQuery(CPU_COUNTER_OBJECT, COUNTER_INSTANCE_TOTAL, CPU_COUNTER_TIME,
			&la->procQuery, &la->procTimeCounter);
		if (status != ERROR_SUCCESS) {
			la->procQuery = NULL;
		} else {
			PdhCollectQueryData(la->procQuery);
		}
		return 0;
	} else {
		PdhCollectQueryData(la->procQuery);
		status = PdhGetFormattedCounterValue(la->procTimeCounter, PDH_FMT_DOUBLE, &dwType, &value);
		if (ERROR_SUCCESS == status) {
			return (value.doubleValue * CpuInfo_GetCpuNum()) / 100.0;
		} else {
			return 0;
		}
	}
}
