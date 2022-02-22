#include "CpuInfo.h"
#include "Counter.h"
#include "Util.h"

CpuInfo* CpuInfo_New()
{
	CpuInfo* cpu = NULL;
	cpu = xMalloc(sizeof(CpuInfo));
	if(NULL == cpu){
		return NULL;
	}
	xMemSet(cpu, 0, sizeof(CpuInfo));
	cpu->init = TRUE;
	return cpu;
}

int CpuInfo_Init(CpuInfo* cpu)
{
	if (NULL == cpu){
		return -1;
	}
	if(cpu->init){
		return 0;
	}
	xMemSet(cpu, 0, sizeof(CpuInfo));
	cpu->init = TRUE;
	return 0;
}

int CpuInfo_GetCpuNum()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return (int)si.dwNumberOfProcessors;
}

int CpuInfo_Read(CpuInfo *cpu) 
{
	PDH_HQUERY query;
	unsigned int threadCount = 0;
	PDH_HCOUNTER threads;
	PPDH_RAW_COUNTER_ITEM values = NULL;
	unsigned int i;
	DWORD dwRet,cbData = sizeof(DWORD);
	HKEY hkey;

	if (PdhOpenQuery(NULL, 0, &query) == ERROR_SUCCESS) {
		PDH_HCOUNTER userTime, systemTime, idleTime, intrTime, interrupts, contexts, uptime, processes;
		if (AddCounterToQuery(CPU_COUNTER_OBJECT, COUNTER_INSTANCE_TOTAL, CPU_COUNTER_USER, &query, &userTime) == ERROR_SUCCESS &&
			AddCounterToQuery(CPU_COUNTER_OBJECT, COUNTER_INSTANCE_TOTAL, CPU_COUNTER_SYSTEM, &query, &systemTime) == ERROR_SUCCESS &&
			AddCounterToQuery(CPU_COUNTER_OBJECT, COUNTER_INSTANCE_TOTAL, CPU_COUNTER_IDLE, &query, &idleTime) == ERROR_SUCCESS &&
			AddCounterToQuery(CPU_COUNTER_OBJECT, COUNTER_INSTANCE_TOTAL, CPU_COUNTER_INTR, &query, &intrTime) == ERROR_SUCCESS &&
			AddCounterToQuery(CPU_COUNTER_OBJECT, COUNTER_INSTANCE_TOTAL, CPU_COUNTER_INTERRUPTS, &query, &interrupts) == ERROR_SUCCESS &&
			AddCounterToQuery(SYS_COUNTER_OBJECT, NULL, SYS_COUNTER_CONTEXTS, &query, &contexts) == ERROR_SUCCESS &&
			AddCounterToQuery(SYS_COUNTER_OBJECT, NULL, SYS_COUNTER_UPTIME, &query, &uptime) == ERROR_SUCCESS &&
			AddCounterToQuery(SYS_COUNTER_OBJECT, NULL, SYS_COUNTER_PROCESSES, &query, &processes) == ERROR_SUCCESS &&
			PdhCollectQueryData(query) == ERROR_SUCCESS) {
			//CPU time is in 100ns units, divide by 10000 for ms
			cpu->cpu_user = (unsigned int)GetRawCounterValue(&userTime)/TICK_TO_MS;
			cpu->cpu_system = (unsigned int)GetRawCounterValue(&systemTime)/TICK_TO_MS;
			cpu->cpu_idle = (unsigned int)GetRawCounterValue(&idleTime)/TICK_TO_MS;
			cpu->cpu_intr = (unsigned int)GetRawCounterValue(&intrTime)/TICK_TO_MS;
			cpu->interrupts = (unsigned int)GetRawCounterValue(&interrupts);
			cpu->contexts = (unsigned int)GetRawCounterValue(&contexts);
			cpu->uptime = (unsigned int)GetCookedCounterValue(&uptime);
			cpu->proc_total = (unsigned int)GetRawCounterValue(&processes);
		}
		PdhCloseQuery(query);
	}
	if (PdhOpenQuery(NULL, 0, &query) == ERROR_SUCCESS) {
		if (AddCounterToQuery(THR_COUNTER_OBJECT, COUNTER_INSTANCE_ALL, THR_COUNTER_STATE, &query, &threads) == ERROR_SUCCESS &&
			PdhCollectQueryData(query) == ERROR_SUCCESS) {
			threadCount = GetRawCounterValues(&threads, &values);
			if (threadCount > 0) {
				for (i = 0; i < threadCount; i++) {
					if (values[i].RawValue.FirstValue == 2 && xStrNCmp("Idle", values[i].szName, 4) != 0) {
						//count the threads that are running (state==2) and are not owned by the idle process.
						//the name of each thread state counter starts with the process name.
						cpu->proc_run++;
						}
				}
				xFree(values);
			}	
		}
		PdhCloseQuery(query);
	}

	cpu->cpu_num = CpuInfo_GetCpuNum();

	// see http://support.microsoft.com/kb/888282 for ways to determine CPU speed
	dwRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
						"hardware\\description\\system\\centralprocessor\\0",
						0,
						KEY_QUERY_VALUE,
						&hkey);

	if(dwRet == ERROR_SUCCESS) {
		dwRet = RegQueryValueEx( hkey,
			                     "~MHz",
			                     NULL,
								 NULL,
			                     (LPBYTE) &cpu->cpu_speed,
			                     &cbData );
		if(dwRet != ERROR_SUCCESS) cpu->cpu_speed = -1;
		RegCloseKey(hkey);
	}

	//These have no obvious Windows equivalent
	cpu->cpu_sintr = UNKNOWN_COUNTER;
	cpu->cpu_nice = UNKNOWN_COUNTER;
	cpu->cpu_wio = UNKNOWN_COUNTER;
	return 0;
}
