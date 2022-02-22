#ifndef _CPUINFO_H_
#define _CPUINFO_H_

typedef struct _CPUINFO_T
{
	unsigned int proc_run;   /* running threads */
	unsigned int proc_total; /* total threads */
	unsigned int cpu_num;    /* # CPU cores */
	unsigned int cpu_speed;  /* speed in MHz of CPU */
	unsigned int uptime;     /* seconds since last reboot */
	unsigned int cpu_user;   /* time executing in user mode processes (ms) */
	unsigned int cpu_nice;   /* time executing niced processs (ms) */
	unsigned int cpu_system; /* time executing kernel mode processes (ms) */
	unsigned int cpu_idle;   /* idle time (ms) */
	unsigned int cpu_wio;    /* time waiting for I/O to complete (ms) */
	unsigned int cpu_intr;   /* time servicing interrupts (ms) */
	unsigned int cpu_sintr;  /* time servicing softirqs (ms) */
	unsigned int interrupts; /* interrupt count */
	unsigned int contexts;   /* context switch count */
	int init;
}CpuInfo;

#ifdef __cplusplus
extern "C"
{
#endif

CpuInfo* CpuInfo_New();
int CpuInfo_Init(CpuInfo*);
int CpuInfo_Read(CpuInfo*);
int CpuInfo_GetCpuNum();

#ifdef __cplusplus
}
#endif

#endif
