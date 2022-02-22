#include "CpuInfo.h"
#include "UpTime.h"
#include "Util.h"

UpTime* UpTime_New()
{
	UpTime* ut = NULL;
	ut = xMalloc(sizeof(UpTime));
	if(NULL == ut){
		return NULL;
	}
	xMemSet(ut, 0, sizeof(UpTime));
	ut->NtQuerySystemInformation = (NTQUERYSYSTEMINFOMATION) GetProcAddress(
		LoadLibrary("ntdll.dll"), "NtQuerySystemInformation");
	if (NULL == ut->NtQuerySystemInformation){
		xFree(ut);
		return NULL;
	}
	ut->init = TRUE;
	return ut;
}

int UpTime_Init(UpTime* ut)
{
	if (NULL == ut){
		return -1;
	}
	if (ut->init){
		return 0;
	}
	xMemSet(ut, 0, sizeof(UpTime));
	ut->NtQuerySystemInformation = (NTQUERYSYSTEMINFOMATION) GetProcAddress(
		LoadLibrary("ntdll.dll"), "NtQuerySystemInformation");
	if (NULL == ut->NtQuerySystemInformation){
		return -1;
	}
	ut->init = TRUE;
	return 0;
}

int UpTime_Read(UpTime *ut) 
{
	DWORD status = NO_ERROR;
	SYSTEM_PERFORMANCE_INFORMATION  SysPerformanceInfo;
	SYSTEM_TIME_INFORMATION        SysTimeInfo;

	if (!ut->init){
		return -1;
	}
	status = ut->NtQuerySystemInformation(SystemTimeInformation, &SysTimeInfo, 
		sizeof(SYSTEM_TIME_INFORMATION), NULL);
	if (NO_ERROR != status){
		return status;
	}
	status = ut->NtQuerySystemInformation(SystemPerformanceInformation, &SysPerformanceInfo, 
		sizeof(SYSTEM_PERFORMANCE_INFORMATION), NULL);
	if ( NO_ERROR != status)
	{
		return status;
	}
	if (0 != ut->OldIdleTime)
	{
		ut->IdleTime = Large_Integer_To_Double(SysPerformanceInfo.IdleTime)  - ut->OldIdleTime;
		ut->SystemTime = Large_Integer_To_Double(SysTimeInfo.liKeSystemTime) - ut->OldSystemTime;
		
		ut->IdleTime = ut->IdleTime/ut->SystemTime;
		ut->IdleTime = 100 -  ut->IdleTime*100.0/((double)CpuInfo_GetCpuNum());
	}
	ut->OldIdleTime = Large_Integer_To_Double(SysPerformanceInfo.IdleTime);
	ut->OldSystemTime =Large_Integer_To_Double( SysTimeInfo.liKeSystemTime);
	return status;
}
