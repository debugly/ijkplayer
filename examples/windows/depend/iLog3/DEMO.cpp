#include <stdio.h>
#include <errno.h>

#include "LOG.h"

int test_hello()
{
	LOG		*g = NULL;
	g = CreateLogHandleG();
	if (g == NULL) {
		printf("create iLog3 Handle errno, errno number=[%d]\n", errno);
		return -1;
	}
	printf("create iLog3 Handle success.\n");

	SetLogOutputG(LOG_OUTPUT_FILE, "./test_hello.log", LOG_NO_OUTPUTFUNC);
	SetLogLevelG(LOG_LEVEL_INFO);
	SetLogStylesG(LOG_STYLE_DEFAULT, LOG_NO_STYLEFUNC);

	iLogDebug("hello iLog3, log level is DEBUG.");
	iLogInfo("hello iLog3, log level is INFO");
	iLogWarn("hello iLog3, log level is WARN");
	iLogError("hello iLog3, log level is ERROR");
	iLogFata("hello iLog3, log level is FATAL");

	int x = 100; float y = 45.8; char z[] = "my first log test";

	iLogDebug("hello iLog3, log level is DEBUGx=%d z=%s y=%f", x, z, y);
	iLogInfo("x=%d z=%s y=%f hello iLog3, log level is INFO", x, z, y);
	iLogWarn("hello iLog3, log level is WARNx=%d z=%s y=%f", x, z, y);
	iLogError("hello iLog3, log level is ERRORx=%d z=%s y=%f", x, z, y);
	iLogFata("hello iLog3, log level is FATALx=%d z=%s y=%f", x, z, y);
	iLogError("hello iLog3, x=%d z=%s y=%f", x, z, y);

	DestroyLogHandleG();
	printf("destroy iLog3 handle.\n");

	return 0;
}

int main()
{
	test_hello();
	system("pause");
}
