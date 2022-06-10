#pragma once

#include <windows.h>
#include <string>
#include <vector>

#define UM_PLAYER_BUFFERING			WM_USER + 1
#define UM_PLAYER_READY				WM_USER + 2
#define UM_PLAYER_PLAYBEGIN			WM_USER + 3
#define UM_PLAYER_STATECHANGE		WM_USER + 4
#define UM_PLAYER_PLAYEND			WM_USER + 5

enum class EMediaType
{
	Unknown = -1,
	Video,
	Audio,
};

namespace IJKPlayer
{
	// control
	bool initialize(const std::vector<std::string>& playerArgs, /*const wchar_t* clearCacheBeforeDate,*/ const wchar_t* logPath /*const wchar_t* preloadDir,*/);
	void uninitialize();

	void setVout(HWND hwnd);
	
	void prepare(const std::string& url);
	void start();
	void play(const std::string& url);
	void pause();
	void stop();
	int seek(long position);

	long getCurrent();
	long getDuration();
	int setVolume(float volume);
	float getVolume();
	bool isPlaying();
	int setPlaybackRate(float rate);
	// stream
	void getSubtitleStreams();
	int setSubtitleStream(int index);
	// subtitle func
	int setSubtitleFontName(const std::string& name);
	std::string getSubtitleFontName();
	int setSubtitle(const std::string& file_name);
	/*only load ex-subtitle, not apply*/
	int loadSubtitle(const std::string& file_name);
	int setSubtitleFontSize(float size);
	float getSubtitleFontSize();
	int setSubtitleDelay(float delay);
	float getSubtitleExtraDelay();
	// snapshot
	// test for now
	void takeSnapshot();
	// parse
	EMediaType MediaTypeSniffing(const std::string& file_name);
}