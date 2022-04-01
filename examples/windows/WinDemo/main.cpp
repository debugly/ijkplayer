#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "../../../ijkmedia/ijkplayer/windows/ijk_ffplay_decoder.h"
#include "logging.h"

#define WIN32_WINDOW

IjkFfplayDecoder *ijk_ffplay_decoder;
IjkFfplayDecoderCallBack *decoder_callback;

static bool  view_init_flag = false;

static char* nv12_data = NULL;

HWND hwnd = NULL;

#ifdef SDL_WINDOW

#pragma comment(lib,"SDL2.lib")
#pragma comment(lib,"SDL2main.lib")

#include "SDL.h"
#include "SDL_syswm.h"

static SDL_Window   *screen;
static SDL_Renderer *sdlRenderer;
static SDL_Texture  *sdlTexture;
static SDL_Rect     sdlRect;

#endif

void msg_callback(void* opaque, IjkMsgState ijk_msgint, int arg1, int arg2)
{
	long duration = 0;
	switch (ijk_msgint)
	{
	case IJK_MSG_VIDEO_DECODE_FPS:
		//Log::Info("video decode fps:%d", arg1);
		break;
	case IJK_MSG_VIDEO_GOP_SIZE:
		//Log::Info("video gop size:%d", arg1);
		break;
	case IJK_MSG_FLUSH:
		break;
	case IJK_MSG_ERROR:
		printf("ijk error, arg1:%d, arg2:%d\n", arg1, arg2);
		break;
	case IJK_MSG_PREPARED:
		ijkFfplayDecoder_start(ijk_ffplay_decoder);
		printf("recv ijk msg:prepared\n");
		break;
	case IJK_MSG_COMPLETED:
		printf("recv ijk msg:complete.\n");
		ijkFfplayDecoder_pause(ijk_ffplay_decoder);
		ijkFfplayDecoder_stop(ijk_ffplay_decoder);
		break;
	case IJK_MSG_VIDEO_SIZE_CHANGED:
		break;
	case IJK_MSG_SAR_CHANGED:
		break;
	case IJK_MSG_VIDEO_RENDERING_START:
		break;
	case IJK_MSG_AUDIO_RENDERING_START:
		break;
	case IJK_MSG_VIDEO_ROTATION_CHANGED:
		break;
	case IJK_MSG_BUFFERING_START:
		break;
	case IJK_MSG_BUFFERING_END:
		break;
	case IJK_MSG_BUFFERING_UPDATE:
		break;
	case IJK_MSG_BUFFERING_BYTES_UPDATE:
		break;
	case IJK_MSG_BUFFERING_TIME_UPDATE:
		break;
	case IJK_MSG_SEEK_COMPLETE:
		break;
	case IJK_MSG_PLAYBACK_STATE_CHANGED:
		break;
	case IJK_MSG_TIMED_TEXT:
		break;
	case IJK_MSG_ACCURATE_SEEK_COMPLETE:
		break;
	case IJK_MSG_VIDEO_DECODER_OPEN:
		break;
	default:
		break;
	}
}

void print_help_info()
{
	printf("\nPrint help / information \n");
	printf("O: open file.\n");
	printf("S: stop play.\n");
	printf("Q: quit demo.\n");
	printf("enter yuor choice now: ");
}

void play_one(const char* path)
{
	// destroy old
	ijkFfplayDecoder_pause(ijk_ffplay_decoder);
	ijkFfplayDecoder_stop(ijk_ffplay_decoder);

	Sleep(500);

	ijkFfplayDecoder_release(ijk_ffplay_decoder);
	// create new
	
	ijk_ffplay_decoder = ijkFfplayDecoder_create();
	ijkFfplayDecoder_setDecoderCallBack(ijk_ffplay_decoder, NULL, decoder_callback);
	ijkFfplayDecoder_setOptionStringValue(ijk_ffplay_decoder, OPT_CATEGORY_FORMAT, "protocol_whitelist", "concat,file,http,https,tcp,tls,crypto,data");
		
	ijkFfplayDecoder_setDataSource(ijk_ffplay_decoder, path);
	ijkFfplayDecoder_prepare(ijk_ffplay_decoder);

	ijkFfplayDecoder_setWindowHwnd(ijk_ffplay_decoder, hwnd);

	int i = 0;
	while (++i < 10)
	{
		Sleep(1000);
	}
}

void test_media_list(const char* listPath)
{
	if (strstr(listPath, "http://") || strstr(listPath, "https://"))
	{
		printf("don't support for now\n");
	}
	// local
	else
	{
		FILE *fp = fopen(listPath, "r");
		if (NULL == fp)
		{
			printf("failed to open list!\n");
			return;
		}

		int len = 0;
		char szPrefix[1024];
		char szPath[1024] = { 0 };

		ShowWindow(hwnd, SW_SHOW);

		while (!feof(fp))
		{
			if (len > 0)
			{
				char tempPath[256] = { 0 };
				strcpy(szPath, szPrefix);
				fgets(tempPath, sizeof(tempPath) - 1, fp);
				strcat(szPath, tempPath);
				// \n
				memset(szPath + strlen(szPath) - 1, 0, sizeof(char));
				play_one(szPath);
			}
			else
			{
				memset(szPrefix, 0, sizeof(szPrefix));
				fgets(szPrefix, sizeof(szPrefix) - 1, fp);
				// \n
				memset(szPrefix + strlen(szPrefix) - 1, 0, sizeof(char));
			}

			++len;
		}

		fclose(fp);
	}
}

static void log_callback(void *, int level, const char * szFmt, va_list varg)
{
	char line[1024] = { 0 };
	vsnprintf(line, sizeof(line), szFmt, varg);

	switch (level) {
	case k_IJK_LOG_DEBUG:
		//Log::Debug("%s", line);
		break;
	case k_IJK_LOG_INFO:
		Log::Info("%s", line);
		break;
	case k_IJK_LOG_WARN:
		Log::Warn("%s", line);
		break;
	case k_IJK_LOG_ERROR:
		Log::Error("%s", line);
		break;
	case k_IJK_LOG_FATAL:
		Log::Fatal("%s", line);
		break;
	default:
		Log::Info("%s", line);
		break;
	}
	return;
}

#ifdef SDL_WINDOW

void video_callback(void* opaque, IjkVideoFrame *frame_callback)
{
	if (!view_init_flag) {
		view_init_flag = true;
		int screen_w, screen_h;

		if (frame_callback->format == PIX_FMT_YUV420P) {
			screen_w = frame_callback->w;
			screen_h = frame_callback->h;
		}
		else if (frame_callback->format == PIX_FMT_NV12) {
			screen_w = frame_callback->linesize[0];
			screen_h = frame_callback->h;
		}

		//screen = SDL_CreateWindow("Simplest ffmpeg player Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_w, screen_h, SDL_WINDOW_OPENGL);
		//if (!screen) {
		//	Log::Info("SDL: could not set video mode - exiting:%s\n", SDL_GetError());
		//	return ;
		//}

		if (frame_callback->format == PIX_FMT_YUV420P) {
			sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
			SDL_RendererInfo renderinfo;
			SDL_GetRendererInfo(sdlRenderer, &renderinfo);
			sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, screen_w, screen_h);
		}
		else if (frame_callback->format == PIX_FMT_NV12) {
			SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
			sdlRenderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED);
			sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_NV12, SDL_TEXTUREACCESS_STREAMING, screen_w, screen_h);
		}

		sdlRect.x = 0;
		sdlRect.y = 0;
		sdlRect.w = screen_w;
		sdlRect.h = screen_h;
	}

	if (frame_callback->format == PIX_FMT_YUV420P) {
		SDL_UpdateYUVTexture(sdlTexture, &sdlRect,
			frame_callback->data[0], frame_callback->linesize[0],
			frame_callback->data[1], frame_callback->linesize[1],
			frame_callback->data[2], frame_callback->linesize[2]);
	}
	else if (frame_callback->format == PIX_FMT_NV12) {
		memcpy(nv12_data, frame_callback->data[0], frame_callback->h*frame_callback->linesize[0]);
		memcpy(nv12_data + frame_callback->h*frame_callback->linesize[0], frame_callback->data[1], frame_callback->h*frame_callback->linesize[0] / 2);
		SDL_UpdateTexture(sdlTexture, NULL, nv12_data, frame_callback->linesize[0]);
	}

	//float dec_fps = ijkFfplayDecoder_getPropertyFloat(ijk_ffplay_decoder, FLOAT_VIDEO_DECODE_FRAMES_PER_SECOND, 0);
	//float out_fps = ijkFfplayDecoder_getPropertyFloat(ijk_ffplay_decoder, FLOAT_VIDEO_OUTPUT_FRAMES_PER_SECOND, 0);
	//float pla_fps = ijkFfplayDecoder_getPropertyFloat(ijk_ffplay_decoder, FLOAT_PLAYBACK_RATE, 0);
	//float delay = ijkFfplayDecoder_getPropertyFloat(ijk_ffplay_decoder, FLOAT_AVDELAY, 0);
	//float diff = ijkFfplayDecoder_getPropertyFloat(ijk_ffplay_decoder, FLOAT_AVDIFF, 0);
	//Log::Info("dec:%f,out:%f,play:%f,delay:%f,diff:%f", dec_fps, out_fps, pla_fps, delay, diff);

	SDL_RenderClear(sdlRenderer);
	SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &sdlRect);
	SDL_RenderPresent(sdlRenderer);
}

int main(int argc, char** argv)
{
	nv12_data = (char*)malloc(32 * 1024 * 1024);

	char listPath[1024] = { 0 };

	std::string url = "./ijkDemo.log";
	Log::Initialise(url);
	Log::SetThreshold(Log::LOG_TYPE_DEBUG);

	//init global paraments
	ijkFfplayDecoder_init();

	ijkFfplayDecoder_setLogLevel(k_IJK_LOG_DEBUG);
	ijkFfplayDecoder_setLogCallback(log_callback);
	decoder_callback = (IjkFfplayDecoderCallBack *)malloc(sizeof(IjkFfplayDecoderCallBack));
	decoder_callback->func_get_frame = video_callback;
	decoder_callback->func_state_change = msg_callback;

	//create ijkplayer and sdl
	ijk_ffplay_decoder = ijkFfplayDecoder_create();
	ijkFfplayDecoder_setDecoderCallBack(ijk_ffplay_decoder, NULL, decoder_callback);
	ijkFfplayDecoder_setOptionStringValue(ijk_ffplay_decoder, OPT_CATEGORY_FORMAT, "protocol_whitelist", "concat,file,http,https,tcp,tls,crypto,data");

	view_init_flag = false;
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
	screen = SDL_CreateWindow("Simplest ffmpeg player Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_OPENGL);
	if (!screen) {
		Log::Info("SDL: could not set video mode - exiting:%s\n", SDL_GetError());
		goto QUIT;
	}

	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(screen, &wmInfo);
	hwnd = wmInfo.info.win.window;

	int mode = 0;
	printf("\nPlease chose your decode mode: \n");
	printf("1: h264_cuvid for nvida\n");
	printf("2: h264_qsv for intel\n");
	printf("3: ffmpeg\n");
	printf("9: auto test mode\n");
	printf("decode mode: ");
	scanf("%d", &mode);
	switch (mode)
	{
	case 1:
		ijkFfplayDecoder_setHwDecoderName(ijk_ffplay_decoder, "h264_cuvid");
		break;
	case 2:
		ijkFfplayDecoder_setHwDecoderName(ijk_ffplay_decoder, "h264_qsv");
		break;
	case 9:
		printf("\nPlease input test list path:\n");
		scanf("%s", listPath);
		goto AUTOTEST;
		break;
	}

	//ijkFfplayDecoder_setDropFrameNums(ijk_ffplay_decoder, 5);
	print_help_info();

	static float volume = 50.0;
	static bool  is_pause = false;
	static int   file_index = 1;
	while (1) {

		char input = ' ';
		scanf("%c", &input);

		//pause and resume play
		if (input == 'p') {
			if (!is_pause) {
				printf("pause player now.\n");
				is_pause = true;
				ijkFfplayDecoder_pause(ijk_ffplay_decoder);

				Sleep(50);

				bool ret = ijkFfplayDecoder_isPlaying(ijk_ffplay_decoder);
				printf("ijkFfplayDecoder_isPlaying: %s.\n", ret ? "true" : "false");
			}
			else {
				printf("resume player now.\n");
				is_pause = false;
				ijkFfplayDecoder_start(ijk_ffplay_decoder);

				Sleep(50);

				bool ret = ijkFfplayDecoder_isPlaying(ijk_ffplay_decoder);
				printf("ijkFfplayDecoder_isPlaying: %s.\n", ret ? "true" : "false");
			}
		}

		//increase sound, 1 percent
		if (input == '+') {
			if (volume < 100) {
				volume += 10;
				printf("increase volume now.\n");
				ijkFfplayDecoder_setVolume(ijk_ffplay_decoder, volume);
			}
			else {
				printf("volume is bigest already.\n");
			}
		}

		//decrease sound
		if (input == '-') {
			if (volume >= 10) {
				volume -= 10;
				printf("decrease volume now.\n");
				ijkFfplayDecoder_setVolume(ijk_ffplay_decoder, volume);
			}
			else {
				printf("volume is zero already.\n");
			}
		}

		//get info: duration, position, video info and audio info
		if (input == 'g') {
			//get volume
			float volume = ijkFfplayDecoder_getVolume(ijk_ffplay_decoder);
			printf("get volume:%f\n", volume);

			//current position and duration
			long position = ijkFfplayDecoder_getCurrentPosition(ijk_ffplay_decoder);
			long duration = ijkFfplayDecoder_getDuration(ijk_ffplay_decoder);
			printf("position:%d, duration:%d.\n", position, duration);

			//code info
			char *videoinfo = (char*)malloc(512);
			char *audioinfo = (char*)malloc(512);
			ijkFfplayDecoder_getVideoCodecInfo(ijk_ffplay_decoder, &videoinfo);
			ijkFfplayDecoder_getAudioCodecInfo(ijk_ffplay_decoder, &audioinfo);
			printf("videoinfo:%s, audioinfo:%s.\n", videoinfo, audioinfo);
			free(videoinfo), free(audioinfo);

			//video info
			float out_frame_rate = ijkFfplayDecoder_getPropertyFloat(ijk_ffplay_decoder, FLOAT_VIDEO_OUTPUT_FRAMES_PER_SECOND, 0);
			long total_bit_rate = ijkFfplayDecoder_getPropertyLong(ijk_ffplay_decoder, INT64_BIT_RATE_TOTAL, 0);
			printf("out frame rate:%f, total_bit_rate:%d Kb/s.\n", out_frame_rate, total_bit_rate / 1000);

			//metadata
			IjkMetadata metadata;
			memset(&metadata, 0, sizeof(IjkMetadata));
			ijkFfplayDecoder_getMediaMeta(ijk_ffplay_decoder, &metadata);
			printf("width:%d, height:%d", metadata.width, metadata.height);
		}

		//seek, default seek to 15s position
		if (input == 's') {
			printf("seek to 15s position.\n");
			ijkFfplayDecoder_seekTo(ijk_ffplay_decoder, 15000);
		}

		//stop
		if (input == 'S') {
			printf("stop ijkplayer now.\n");

			ijkFfplayDecoder_pause(ijk_ffplay_decoder);
			ijkFfplayDecoder_stop(ijk_ffplay_decoder);

		}

		//quit ijkplayer
		if (input == 'Q'|| input == 'q') {
			ijkFfplayDecoder_release(ijk_ffplay_decoder);
			printf("quit now.\n");
			goto QUIT;
		}

		//open file, default test.flv in current direct
		if (input == 'O') {
			char path[1024] = { 0 };//"rtsp://192.168.31.243/stream12";//{ 0 };
			printf("\nPlease input file path:\n");
			scanf("%s", path);

			ijkFfplayDecoder_setDataSource(ijk_ffplay_decoder, path);
			ijkFfplayDecoder_prepare(ijk_ffplay_decoder);
			
			ijkFfplayDecoder_setWindowHwnd(ijk_ffplay_decoder, hwnd);
		}

		if (input == 'd') {
			int mode = 0;
			printf("\nPlease chose your decode mode: \n");
			printf("1: h264_cuvid for nvida\n");
			printf("2: h264_qsv for intel\n");
			printf("3: ffmpeg\n");
			printf("decode mode: ");
			scanf("%d", &mode);
			switch (mode)
			{
			case 1:
				ijkFfplayDecoder_setHwDecoderName(ijk_ffplay_decoder, "h264_cuvid");
				break;
			case 2:
				ijkFfplayDecoder_setHwDecoderName(ijk_ffplay_decoder, "h264_qsv");
				break;
			case 3:
				ijkFfplayDecoder_setHwDecoderName(ijk_ffplay_decoder, NULL);
				break;
			}
			print_help_info();
		}

		Sleep(500);
	}

AUTOTEST:
	test_media_list(listPath);
QUIT:
	ijkFfplayDecoder_uninit();

	SDL_DestroyTexture(sdlTexture);
	SDL_DestroyRenderer(sdlRenderer);
	SDL_DestroyWindow(screen);
	SDL_Quit();

	if (nv12_data) {
		free(nv12_data);
	}
	system("pause");
	return 0;
}
#endif // SDL_WINDOW

#ifdef WIN32_WINDOW

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

HWND CreateVideoWindow()
{
	const wchar_t* CLASS_NAME = L"Window Class";
	HINSTANCE hInstance = NULL;

	WNDCLASS wc = {};

	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	
	RegisterClass(&wc);

	hwnd = CreateWindowEx(
		0,                             
		CLASS_NAME,                    
		L"Demo",						
		WS_OVERLAPPEDWINDOW,            
		CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, 
		NULL,       
		NULL,       
		hInstance,  
		NULL        
	);

	ShowWindow(hwnd, SW_HIDE);
	UpdateWindow(hwnd);

	return hwnd;
}

DWORD _stdcall CreateThreadWindow(PVOID pArg)
{
	hwnd = CreateVideoWindow();

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

int main()
{
	char listPath[1024] = { 0 };
	
	std::string url = "./ijkDemo.log";
	Log::Initialise(url);
	Log::SetThreshold(Log::LOG_TYPE_DEBUG);

	//init global paraments
	ijkFfplayDecoder_init();

	ijkFfplayDecoder_setLogLevel(k_IJK_LOG_DEBUG);
	ijkFfplayDecoder_setLogCallback(log_callback);
	
	decoder_callback = (IjkFfplayDecoderCallBack *)malloc(sizeof(IjkFfplayDecoderCallBack));
	decoder_callback->func_get_frame = /*video_callback*/NULL;
	decoder_callback->func_state_change = msg_callback;

	//create ijkplayer and sdl
	ijk_ffplay_decoder = ijkFfplayDecoder_create();
	ijkFfplayDecoder_setDecoderCallBack(ijk_ffplay_decoder, NULL, decoder_callback);
	ijkFfplayDecoder_setOptionStringValue(ijk_ffplay_decoder, OPT_CATEGORY_FORMAT, "protocol_whitelist", "concat,file,http,https,tcp,tls,crypto,data");

	int mode = 0;

	HANDLE hThrd = CreateThread(NULL, 0, CreateThreadWindow, NULL, 0, NULL);
	CloseHandle(hThrd);

	printf("\nPlease chose your decode mode: \n");
	printf("1: h264_cuvid for nvida\n");
	printf("2: h264_qsv for intel\n");
	printf("3: ffmpeg\n");
	printf("9: auto test mode\n");
	printf("decode mode: ");
	scanf("%d", &mode);
	switch (mode)
	{
	case 1:
		ijkFfplayDecoder_setHwDecoderName(ijk_ffplay_decoder, "h264_cuvid");
		break;
	case 2:
		ijkFfplayDecoder_setHwDecoderName(ijk_ffplay_decoder, "h264_qsv");
		break;
	case 9:
		printf("\nPlease input test list path:\n");
		scanf("%s", listPath);
		goto AUTOTEST;
	}

	//ijkFfplayDecoder_setDropFrameNums(ijk_ffplay_decoder, 5);
	print_help_info();

	static float volume = 50.0;
	static bool  is_pause = false;
	static int   file_index = 1;
	while (1) {

		char input = ' ';
		scanf("%c", &input);

		//pause and resume play
		if (input == 'p') {
			if (!is_pause) {
				printf("pause player now.\n");
				is_pause = true;
				ijkFfplayDecoder_pause(ijk_ffplay_decoder);

				Sleep(50);

				bool ret = ijkFfplayDecoder_isPlaying(ijk_ffplay_decoder);
				printf("ijkFfplayDecoder_isPlaying: %s.\n", ret ? "true" : "false");
			}
			else {
				printf("resume player now.\n");
				is_pause = false;
				ijkFfplayDecoder_start(ijk_ffplay_decoder);

				Sleep(50);

				bool ret = ijkFfplayDecoder_isPlaying(ijk_ffplay_decoder);
				printf("ijkFfplayDecoder_isPlaying: %s.\n", ret ? "true" : "false");
			}
		}

		//increase sound, 1 percent
		if (input == '+') {
			if (volume < 100) {
				volume += 10;
				printf("increase volume now.\n");
				ijkFfplayDecoder_setVolume(ijk_ffplay_decoder, volume);
			}
			else {
				printf("volume is bigest already.\n");
			}
		}

		//decrease sound
		if (input == '-') {
			if (volume >= 10) {
				volume -= 10;
				printf("decrease volume now.\n");
				ijkFfplayDecoder_setVolume(ijk_ffplay_decoder, volume);
			}
			else {
				printf("volume is zero already.\n");
			}
		}

		//get info: duration, position, video info and audio info
		if (input == 'g') {
			//get volume
			float volume = ijkFfplayDecoder_getVolume(ijk_ffplay_decoder);
			printf("get volume:%f\n", volume);

			//current position and duration
			long position = ijkFfplayDecoder_getCurrentPosition(ijk_ffplay_decoder);
			long duration = ijkFfplayDecoder_getDuration(ijk_ffplay_decoder);
			printf("position:%d, duration:%d.\n", position, duration);

			//code info
			char *videoinfo = (char*)malloc(512);
			char *audioinfo = (char*)malloc(512);
			ijkFfplayDecoder_getVideoCodecInfo(ijk_ffplay_decoder, &videoinfo);
			ijkFfplayDecoder_getAudioCodecInfo(ijk_ffplay_decoder, &audioinfo);
			printf("videoinfo:%s, audioinfo:%s.\n", videoinfo, audioinfo);
			free(videoinfo), free(audioinfo);

			//video info
			float out_frame_rate = ijkFfplayDecoder_getPropertyFloat(ijk_ffplay_decoder, FLOAT_VIDEO_OUTPUT_FRAMES_PER_SECOND, 0);
			long total_bit_rate = ijkFfplayDecoder_getPropertyLong(ijk_ffplay_decoder, INT64_BIT_RATE_TOTAL, 0);
			printf("out frame rate:%f, total_bit_rate:%d Kb/s.\n", out_frame_rate, total_bit_rate / 1000);

			//metadata
			IjkMetadata metadata;
			memset(&metadata, 0, sizeof(IjkMetadata));
			ijkFfplayDecoder_getMediaMeta(ijk_ffplay_decoder, &metadata);
			printf("width:%d, height:%d", metadata.width, metadata.height);
		}

		//seek, default seek to 15s position
		if (input == 's') {
			printf("seek to 15s position.\n");
			ijkFfplayDecoder_seekTo(ijk_ffplay_decoder, 15000);
		}

		//stop
		if (input == 'S') {
			printf("stop ijkplayer now.\n");

			ijkFfplayDecoder_pause(ijk_ffplay_decoder);
			ijkFfplayDecoder_stop(ijk_ffplay_decoder);
		}

		//quit ijkplayer
		if (input == 'Q' || input == 'q') {
			ijkFfplayDecoder_release(ijk_ffplay_decoder);
			printf("quit now.\n");
			goto QUIT;
		}

		//open file, default test.flv in current direct
		if (input == 'O') {
			char path[1024] = { 0 };//"rtsp://192.168.31.243/stream12";//{ 0 };
			printf("\nPlease input file path:\n");
			scanf("%s", path);
			view_init_flag = false;
			
			ijkFfplayDecoder_setDataSource(ijk_ffplay_decoder, path);
			ijkFfplayDecoder_prepare(ijk_ffplay_decoder);

			ijkFfplayDecoder_setWindowHwnd(ijk_ffplay_decoder, hwnd);

			ShowWindow(hwnd, SW_SHOW);
		}

		if (input == 'd') {
			int mode = 0;
			printf("\nPlease chose your decode mode: \n");
			printf("1: h264_cuvid for nvida\n");
			printf("2: h264_qsv for intel\n");
			printf("3: ffmpeg\n");
			printf("decode mode: ");
			scanf("%d", &mode);
			switch (mode)
			{
			case 1:
				ijkFfplayDecoder_setHwDecoderName(ijk_ffplay_decoder, "h264_cuvid");
				break;
			case 2:
				ijkFfplayDecoder_setHwDecoderName(ijk_ffplay_decoder, "h264_qsv");
				break;
			case 3:
				ijkFfplayDecoder_setHwDecoderName(ijk_ffplay_decoder, NULL);
				break;
			}
			print_help_info();
		}

		Sleep(500);
	}
AUTOTEST:
	test_media_list(listPath);
QUIT:
	ijkFfplayDecoder_uninit();
	if (nv12_data) {
		free(nv12_data);
	}
	system("pause");
	return 0;
}
#endif // WIN32_WINDOW