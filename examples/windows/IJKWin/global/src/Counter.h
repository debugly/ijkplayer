#ifndef _COUNTER_H_
#define _COUNTER_H_

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Pdh.h>

#pragma comment(lib,"Pdh.lib")
#define PDH_MORE_DATA 0x800007D2

#define COUNTER_INSTANCE_TOTAL "_Total"
#define COUNTER_INSTANCE_ALL "*"
#define SYS_COUNTER_OBJECT "System"
#define SYS_COUNTER_PROC_QLEN "Processor Queue Length"
#define SYS_COUNTER_CONTEXTS "Context Switches/sec"
#define SYS_COUNTER_UPTIME "System Up Time"
#define SYS_COUNTER_PROCESSES "Processes"
#define CPU_COUNTER_OBJECT "Processor"
#define CPU_COUNTER_TIME "% Processor Time"
#define CPU_COUNTER_USER "% User Time"
#define CPU_COUNTER_SYSTEM "% Privileged Time"
#define CPU_COUNTER_IDLE "% Idle Time"
#define CPU_COUNTER_INTR "% Interrupt Time"
#define CPU_COUNTER_INTERRUPTS "Interrupts/sec"
#define CPU_COUNTER_TIME "% Processor Time"
#define THR_COUNTER_OBJECT "Thread"
#define THR_COUNTER_STATE "Thread State"
#define MEM_COUNTER_OBJECT "Memory"
#define MEM_COUNTER_CACHE "Cache Bytes"
#define MEM_COUNTER_PAGE_IN "Pages Input/sec"
#define MEM_COUNTER_PAGE_OUT "Pages Output/sec"

#define UNKNOWN_COUNTER    0xFFFFFFFF 
#define UNKNOWN_COUNTER_64 0xFFFFFFFFFFFFFFFF
#define UNKNOWN_GAUGE    0xFFFFFFFF 
#define UNKNOWN_GAUGE_64 0xFFFFFFFFFFFFFFFF

#define TICK_TO_MS 10000;

#ifdef __cplusplus
extern "C"
{
#endif

char* GetCounterPath(char *object, char *instance, char *counter);
LONGLONG GetRawCounterValue(PDH_HCOUNTER *counter);
unsigned int GetRawCounterValues(PDH_HCOUNTER *counter, PPDH_RAW_COUNTER_ITEM *values);
LONGLONG GetCookedCounterValue(PDH_HCOUNTER *counter);
PDH_STATUS AddCounterToQuery(char *object, char *instance, char *counterName,
							 PDH_HQUERY *query, PDH_HCOUNTER *counter);
PDH_STATUS MakeSingleCounterQuery(char *object, char *instance, char *counterName,
								  PDH_HQUERY *query, PDH_HCOUNTER *counter);

#ifdef __cplusplus
}
#endif

#endif
