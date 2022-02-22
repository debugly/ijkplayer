#include "Counter.h"
#include "Util.h"

char* GetCounterPath(char *object, char *instance, char *counter)
{
	char* szFullPath = NULL;
	DWORD cbPathSize = 0;
	PDH_STATUS s;
	PDH_COUNTER_PATH_ELEMENTS cpe;

	cpe.szMachineName = NULL;
	cpe.szObjectName = object;
	cpe.szInstanceName = instance;
	cpe.szParentInstance = NULL;
	cpe.dwInstanceIndex = 0;
	cpe.szCounterName = counter;

	s = PdhMakeCounterPath(&cpe, NULL, &cbPathSize, 0);
	if (s == PDH_MORE_DATA) {
		szFullPath = (char *)xMalloc(cbPathSize*sizeof(char));
		s = PdhMakeCounterPath(&cpe, szFullPath, &cbPathSize, 0);
	}
	return szFullPath;
}

LONGLONG GetRawCounterValue(PDH_HCOUNTER *counter)
{
	DWORD dwType;
	PDH_RAW_COUNTER value;
	if (PdhGetRawCounterValue(*counter, &dwType, &value) == ERROR_SUCCESS) {
		return value.FirstValue;
	}
	return 0;
}

LONGLONG GetCookedCounterValue(PDH_HCOUNTER *counter)
{
	DWORD dwType;
	PDH_FMT_COUNTERVALUE value;
	if (ERROR_SUCCESS == PdhGetFormattedCounterValue(*counter, PDH_FMT_LARGE, &dwType, &value)) {
		return value.largeValue;
	}
	return 0;
}

unsigned int GetRawCounterValues(PDH_HCOUNTER *counter, PPDH_RAW_COUNTER_ITEM *values)
{
	DWORD bufSize = 0;
	DWORD itemCount = 0;
	unsigned int ret = 0;
	if (PdhGetRawCounterArray(*counter, &bufSize, &itemCount, NULL) == PDH_MORE_DATA) {
		*values = (PPDH_RAW_COUNTER_ITEM)xMalloc(bufSize);
		if (PdhGetRawCounterArray(*counter, &bufSize, &itemCount, *values) == ERROR_SUCCESS) {
			ret = itemCount;
			if (ret > 0 && xStrCmp(COUNTER_INSTANCE_TOTAL, values[0][itemCount-1].szName) == 0) {
				ret--; // use readSingleCounter if you need _Total;
			}
		}
	}
	return ret;
}

PDH_STATUS AddCounterToQuery(char *object, char *instance, char *counterName,
							 PDH_HQUERY *query, PDH_HCOUNTER *counter)
{
	char* counterPath = GetCounterPath(object, instance, counterName);
	PDH_STATUS status = PdhAddEnglishCounter(*query, counterPath, 0, counter);
	xFree(counterPath);
	return status;
}


PDH_STATUS MakeSingleCounterQuery(char *object, char *instance, char *counterName,
								  PDH_HQUERY *query, PDH_HCOUNTER *counter)
{
	PDH_STATUS status = PdhOpenQuery(NULL, 0, query);
	if (ERROR_SUCCESS == status) {
		status = AddCounterToQuery(object, instance, counterName, query, counter);
	}
	if (query && ERROR_SUCCESS != status) {
		PdhCloseQuery(*query);
		*query = NULL;
	}
	return status;
}
