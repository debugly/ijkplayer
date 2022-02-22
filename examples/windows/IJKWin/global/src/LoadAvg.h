#ifndef _LOADAVG_H_
#define _LOADAVG_H_

#include "Counter.h"

typedef struct LOADAVG_T{
	double load_1;
	double load_5;
	double load_15;
	PDH_HQUERY procQuery;
	PDH_HCOUNTER procTimeCounter;
}LoadAvg;

#ifdef __cplusplus
extern "C"
{
#endif

LoadAvg* LoadAvg_New();
int LoadAvg_Init(LoadAvg* la);
int LoadAvg_CalcLoad(LoadAvg*);
double LoadAvg_GetCpuLoad(LoadAvg*);

#ifdef __cplusplus
}
#endif

#endif


