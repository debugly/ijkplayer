#include "IJKPlayer.h"
#include <assert.h>
//Todo:TidyUp
#include "../../../ijkmedia/ijkplayer/windows/ijk_ffplay_decoder.h"
#include "logging.h"

extern "C"
{
	#include <libswscale/swscale.h>
}

FILE* _ijk_log_stream = NULL;
IjkFfplayDecoder *_ijk_ffplay_decoder = NULL;
IjkFfplayDecoderCallBack *_decoder_callback = NULL;

HWND _ijk_vout = NULL;

void _log_callback(void *, int level, const char * szFmt, va_list varg)
{
	char line[1024] = { 0 };
	vsnprintf(line, sizeof(line), szFmt, varg);

	switch (level) {
	case k_IJK_LOG_DEBUG:
		Log::Debug("%s", line);
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

void _msg_callback(void* opaque, IjkMsgState message, int arg1, int arg2)
{
	switch (message)
	{
	case IJK_MSG_VIDEO_DECODE_FPS:
		OutputDebugString(L"hint IJK_MSG_VIDEO_DECODE_FPS\n");
		break;
	case IJK_MSG_VIDEO_GOP_SIZE:
		OutputDebugString(L"hint IJK_MSG_VIDEO_GOP_SIZE\n");
		break;
	case IJK_MSG_FLUSH:
		PostMessage(_ijk_vout, UM_PLAYER_BUFFERING, NULL, NULL);
		break;
	case IJK_MSG_ERROR:
		OutputDebugString(L"hint IJK_MSG_ERROR\n");
		break;
	case IJK_MSG_PREPARED:
		PostMessage(_ijk_vout, UM_PLAYER_READY, NULL, NULL);
		break;
	case IJK_MSG_COMPLETED:
		PostMessage(_ijk_vout, UM_PLAYER_PLAYEND, NULL, NULL);
		break;
	case IJK_MSG_VIDEO_SIZE_CHANGED:
		OutputDebugString(L"hint IJK_MSG_VIDEO_SIZE_CHANGED\n");
		break;
	case IJK_MSG_SAR_CHANGED:
		OutputDebugString(L"hint IJK_MSG_SAR_CHANGED\n");
		break;
	case IJK_MSG_VIDEO_RENDERING_START:
		PostMessage(_ijk_vout, UM_PLAYER_PLAYBEGIN, NULL, NULL);
		break;
	case IJK_MSG_AUDIO_RENDERING_START:
		PostMessage(_ijk_vout, UM_PLAYER_PLAYBEGIN, NULL, NULL);
		break;
	case IJK_MSG_VIDEO_ROTATION_CHANGED:
		OutputDebugString(L"hint IJK_MSG_VIDEO_ROTATION_CHANGED\n");
		break;
	case IJK_MSG_BUFFERING_START:
		OutputDebugString(L"hint IJK_MSG_BUFFERING_START\n");
		break;
	case IJK_MSG_BUFFERING_END:
		OutputDebugString(L"hint IJK_MSG_BUFFERING_END\n");
		break;
	case IJK_MSG_BUFFERING_UPDATE:
		OutputDebugString(L"hint IJK_MSG_BUFFERING_UPDATE\n");
		break;
	case IJK_MSG_BUFFERING_BYTES_UPDATE:
		OutputDebugString(L"hint IJK_MSG_BUFFERING_BYTES_UPDATE\n");
		break;
	case IJK_MSG_BUFFERING_TIME_UPDATE:
		OutputDebugString(L"hint IJK_MSG_BUFFERING_TIME_UPDATE\n");
		break;
	case IJK_MSG_SEEK_COMPLETE:
		OutputDebugString(L"hint IJK_MSG_SEEK_COMPLETE\n");
		break;
	case IJK_MSG_PLAYBACK_STATE_CHANGED:
		PostMessage(_ijk_vout, UM_PLAYER_STATECHANGE, NULL, NULL);
		break;
	case IJK_MSG_TIMED_TEXT:
		OutputDebugString(L"hint IJK_MSG_TIMED_TEXT\n");
		break;
	case IJK_MSG_ACCURATE_SEEK_COMPLETE:
		OutputDebugString(L"hint IJK_MSG_ACCURATE_SEEK_COMPLETE\n");
		break;
	case IJK_MSG_VIDEO_DECODER_OPEN:
		OutputDebugString(L"hint IJK_MSG_VIDEO_DECODER_OPEN\n");
		break;
	default:
		break;
	}
}

void _video_callback(void* opaque, IjkVideoFrame *frame)
{
	//static int count = 24;
	//if (--count == 0)
	//{
	//	assert(frame->format == AV_PIX_FMT_YUV420P);

	//	SwsContext* swsContext = sws_getContext(frame->w, frame->h, AV_PIX_FMT_YUV420P, frame->w, frame->h, AV_PIX_FMT_BGR24, NULL, NULL, NULL, NULL);
	//	int channel = 3;
	//	int linesize[8] = { frame->linesize[0] * channel };
	//	int num_bytes = frame->w * frame->h * 1 * channel;
	//	uint8_t* buffer = (uint8_t*)malloc(num_bytes * sizeof(uint8_t));
	//	uint8_t* rgb_buffer[8] = { buffer };
	//	sws_scale(swsContext, frame->data, frame->linesize, 0, frame->h, rgb_buffer, linesize);
	//	
	//	int width = frame->w;
	//	int height = frame->h;

	//	BITMAPFILEHEADER bmpheader;
	//	BITMAPINFO bmpinfo;
	//	FILE *pFile;
	//	char szFilename[32];

	//	sprintf(szFilename, "D:\\0.bmp");
	//	pFile = fopen(szFilename, "wb");
	//	bmpheader.bfType = 0x4d42;
	//	bmpheader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	//	bmpheader.bfSize = bmpheader.bfOffBits + width * height * channel;
	//	bmpheader.bfReserved1 = 0;
	//	bmpheader.bfReserved2 = 0;

	//	bmpinfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	//	bmpinfo.bmiHeader.biWidth = width;
	//	bmpinfo.bmiHeader.biHeight = -height;
	//	bmpinfo.bmiHeader.biPlanes = 1;
	//	bmpinfo.bmiHeader.biBitCount = channel * 8;// 24bit BGR
	//	bmpinfo.bmiHeader.biCompression = BI_RGB;
	//	bmpinfo.bmiHeader.biSizeImage = 0;
	//	bmpinfo.bmiHeader.biXPelsPerMeter = 0;
	//	bmpinfo.bmiHeader.biYPelsPerMeter = 0;
	//	bmpinfo.bmiHeader.biClrUsed = 0;
	//	bmpinfo.bmiHeader.biClrImportant = 0;

	//	fwrite(&bmpheader, sizeof(BITMAPFILEHEADER), 1, pFile);
	//	fwrite(&bmpinfo.bmiHeader, sizeof(BITMAPINFOHEADER), 1, pFile);

	//	fwrite(rgb_buffer[0], width * height * channel, 1, pFile);
	//	fclose(pFile);

	//	free(buffer);

	//	buffer = NULL;
	//	rgb_buffer[0] = NULL;

	//	sws_freeContext(swsContext);
	//}
}

void _uninit_decoder()
{
	if (_decoder_callback)
	{
		free(_decoder_callback);
		_decoder_callback = NULL;
	}
	
	if (_ijk_ffplay_decoder)
	{
		ijkFfplayDecoder_pause(_ijk_ffplay_decoder);
		ijkFfplayDecoder_stop(_ijk_ffplay_decoder);

		ijkFfplayDecoder_release(_ijk_ffplay_decoder);
	}
}

bool IJKPlayer::initialize(const std::vector<std::string>& playerArgs, /*const wchar_t* clearCacheBeforeDate,*/ const wchar_t* logPath /*const wchar_t* preloadDir,*/)
{
	ijkFfplayDecoder_init();

	//LPCWSTR szPath(logPath);
	std::string strPath = "./ijkDemo.log";
	
	Log::Initialise(strPath);

	ijkFfplayDecoder_setLogLevel(k_IJK_LOG_DEBUG);
	ijkFfplayDecoder_setLogCallback(_log_callback);
	
	return true;
}

void IJKPlayer::uninitialize()
{
	if (_decoder_callback)
	{
		free(_decoder_callback);
		_decoder_callback = NULL;
	}

	_uninit_decoder();
	_ijk_ffplay_decoder = NULL;

	ijkFfplayDecoder_uninit();
}

void IJKPlayer::setVout(HWND hwnd)
{
	_ijk_vout = hwnd;
}

void IJKPlayer::openCodecHW()
{
	// need UI thread 
}

void IJKPlayer::closeCodecHW()
{
	// need UI thread 
}

void IJKPlayer::prepare(const std::string & url)
{
	_uninit_decoder();

	_decoder_callback = (IjkFfplayDecoderCallBack*)malloc(sizeof(IjkFfplayDecoderCallBack));
	_decoder_callback->func_get_frame = _video_callback;
	_decoder_callback->func_state_change = _msg_callback;

	_ijk_ffplay_decoder = ijkFfplayDecoder_create();
	ijkFfplayDecoder_setDecoderCallBack(_ijk_ffplay_decoder, NULL, _decoder_callback);

	ijkFfplayDecoder_setOptionStringValue(_ijk_ffplay_decoder, OPT_CATEGORY_FORMAT, "protocol_whitelist", "concat,file,http,https,tcp,tls,crypto,data");

	//ijkFfplayDecoder_setHwDecoderName(_ijk_ffplay_decoder, "h264_qsv");

	ijkFfplayDecoder_setDataSource(_ijk_ffplay_decoder, url.c_str());
	ijkFfplayDecoder_prepare(_ijk_ffplay_decoder);
}

void IJKPlayer::start()
{
	ijkFfplayDecoder_start(_ijk_ffplay_decoder);
}

void IJKPlayer::play(const std::string & url)
{
	prepare(url);
	ijkFfplayDecoder_setWindowHwnd(_ijk_ffplay_decoder, _ijk_vout);
}

void IJKPlayer::pause()
{
	ijkFfplayDecoder_pause(_ijk_ffplay_decoder);
}

void IJKPlayer::stop()
{
	ijkFfplayDecoder_pause(_ijk_ffplay_decoder);
	ijkFfplayDecoder_stop(_ijk_ffplay_decoder);
}

int IJKPlayer::seek(long position)
{
	return ijkFfplayDecoder_seekTo(_ijk_ffplay_decoder, position);
}

long IJKPlayer::getCurrent()
{
	return ijkFfplayDecoder_getCurrentPosition(_ijk_ffplay_decoder);
}

long IJKPlayer::getDuration()
{
	return ijkFfplayDecoder_getDuration(_ijk_ffplay_decoder);
}

int IJKPlayer::setVolume(float volume)
{
	return ijkFfplayDecoder_setVolume(_ijk_ffplay_decoder, volume);
}

float IJKPlayer::getVolume()
{
	return ijkFfplayDecoder_getVolume(_ijk_ffplay_decoder);
}

bool IJKPlayer::isPlaying()
{
	return ijkFfplayDecoder_isPlaying(_ijk_ffplay_decoder);
}