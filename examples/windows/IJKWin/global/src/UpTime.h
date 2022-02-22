#ifndef _UPTIME_H_
#define _UPTIME_H_

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

typedef enum SYSTEM_INFORMATION_T
{
	SystemBasicInformation = 0,
	SystemPerformanceInformation = 2,
	SystemTimeInformation  = 3,
}SYSTEM_INFORMATION;

typedef DWORD (WINAPI* NTQUERYSYSTEMINFOMATION) (         
	SYSTEM_INFORMATION SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength );

typedef struct SYSTEM_PERFORMANCE_INFORMATION_T
{
	LARGE_INTEGER IdleTime;
	LARGE_INTEGER ReadTransferCount;
	LARGE_INTEGER WriteTransferCount;
	LARGE_INTEGER OtherTransferCount;
	ULONG ReadOperationCount;
	ULONG WriteOperationCount;
	ULONG OtherOperationCount;
	ULONG AvailablePages;
	ULONG TotalCommittedPages;
	ULONG TotalCommitLimit;
	ULONG PeakCommitment;
	ULONG PageFaults;           // total soft or hard Page Faults since boot (wraps at 32-bits)
	ULONG Reserved[74]; // unknown
}SYSTEM_PERFORMANCE_INFORMATION;

typedef struct SYSTEM_TIME_INFORMATION_T
{
	LARGE_INTEGER liKeBootTime;
	LARGE_INTEGER liKeSystemTime;
	LARGE_INTEGER liExpTimeZoneBias;
	ULONG         uCurrentTimeZoneId;
	DWORD         dwReserved;
} SYSTEM_TIME_INFORMATION;


typedef struct _UPTIME_
{
	double SystemTime;
	double OldSystemTime;
	double IdleTime;
	double OldIdleTime;
	int init;
	NTQUERYSYSTEMINFOMATION NtQuerySystemInformation;
}UpTime;

#define Large_Integer_To_Double(want) ((double)(want.HighPart) * 4.294967296E9 + (double)(want.LowPart))

#ifdef __cplusplus
extern "C"
{
#endif

UpTime* UpTime_New();
int UpTime_Init(UpTime*);
int UpTime_Read(UpTime*);

#ifdef __cplusplus
}
#endif

#endif
