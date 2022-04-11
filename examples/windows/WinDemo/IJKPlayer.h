#pragma once

//Todo:TidyUp
//#include "../../../../include/PlayerInterface.h"
#include <windows.h>
#include <string>
#include <vector>

#define UM_PLAY (WM_USER + 1)

typedef void(*ijk_LogCallback)(void*, int, const char * fmt, va_list args);

namespace IJKPlayer
{
	bool initialize(const std::vector<std::string>& playerArgs, /*const wchar_t* clearCacheBeforeDate,*/ const wchar_t* logPath, /*const wchar_t* preloadDir,*/ /*PlayerCore::*/ijk_LogCallback logCallback);
	void uninitialize();

	void setVout(HWND hwnd);

	void prepare(const std::string& url);
	void start();
	void play(const std::string& url);
	void pause();
	void stop();

	int seek(long position);

	long getDuration();
}