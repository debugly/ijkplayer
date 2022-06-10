//
//  ijk_ffplay_decoder.c
//  IJKMediaPlayer
//
//  Created by chao on 2017/6/12.
//  Copyright © 2017 detu. All rights reserved.
//

#include "ijk_text_demuxer.h"
#include "ijk_sub_decoder.h"
#include "ijksdl\ijksdl_thread.h"
#include "ijkplayer\ff_ffplay.h"
#include "ijkplayer\ijkplayer.h"


typedef struct IjkTextDemuxer {
	PacketQueue subq;
	AVFormatContext *ic;
	AVStream *st;
	int st_idx;

	char* contentPath;
	SDL_Thread* workThread;
	SDL_Thread	_workThread;
	//视频解码器
	IjkSubDecoder *subDecoder;
	//读包完毕？
	bool readEOF;
	int frameCount;
	int pktCount;
	int duration;
	///通过block接收回调
	OnStreamOpenedBlock onStreamOpenedBlock;
	OnFrameComingBlock onFrameComingBlock;
	OnFinishedBlock onFinishedBlock;
	bool convertToAss;
} IjkTextDemuxer;

IjkTextDemuxer* Ijk_createTextDemuxer(const char* content_path)
{
	IjkTextDemuxer* demux = (IjkTextDemuxer*)calloc(sizeof(IjkTextDemuxer), 1);
	if (demux) {
		demux->contentPath = content_path;
	}
	return demux;
}


bool isAbortTextDemuxer(IjkTextDemuxer* demux)
{
	return !demux->workThread;
}

//准备
void Ijk_prepareTextDemuxer(IjkTextDemuxer* demux)
{

}

#pragma mark - 打开解码器创建解码线程
IjkSubDecoder* dumpStreamComponent(AVFormatContext *ic, int idx)
{
	IjkSubDecoder *decoder = (IjkSubDecoder*)calloc(sizeof(IjkSubDecoder), 1);
	decoder->ic = ic;
	decoder->streamIdx = idx;
	ijk_dumpStreamFormat(decoder);

	return decoder;
}

#pragma -mark 读包逻辑
//读包循环
void readPacketLoop(IjkTextDemuxer* demux, AVFormatContext *formatCtx)
{
	AVPacket pkt1, *pkt = &pkt1;
	//循环读包，读满了就停止
	for (;;) {
		//调用了stop方法，则不再读包
		if (isAbortTextDemuxer(demux)) {
			break;
		}

		//已经读完了
		if (demux->readEOF) {
			break;
		}

		/* 队列不满继续读，满了则休眠10 ms */
		if (demux->subq.size > 1 * 1024 * 1024
			|| (ffp_stream_has_enough_packets(demux->st, demux->st_idx, &demux->subq))) {
			break;
		}
		//读包
		int ret = av_read_frame(formatCtx, pkt);
		//读包出错
		if (ret < 0) {
			//读到最后结束了
			if ((ret == AVERROR_EOF || avio_feof(formatCtx->pb)) && !demux->readEOF) {
				//最后放一个空包进去
				if (demux->st_idx >= 0) {
					ffp_packet_queue_put_nullpacket(&demux->subq, demux->st_idx);
				}
				//标志为读包结束
				av_log(NULL, AV_LOG_INFO, "real read eof\n");
				demux->readEOF = true;
				break;
			}

			if (formatCtx->pb && formatCtx->pb->error) {
				break;
			}
			break;
		}
		else {
			//视频包入视频队列
			if (pkt->stream_index == demux->st_idx) {
				if (demux->subDecoder) {
					char* name = ijk_getCodecName(demux->subDecoder);

					//gif 不能按关键帧处理
					if (name != NULL) {
						if (0 != strcmp(name, "gif")) {
							if (!(pkt->flags & AV_PKT_FLAG_KEY)) {
								av_packet_unref(pkt);
								continue;
							}
						}
					}
				}

				//没有pts，则使用dts当做pts
				if (AV_NOPTS_VALUE == pkt->pts && AV_NOPTS_VALUE != pkt->dts) {
					pkt->pts = pkt->dts;
				}

				//lastPkts记录上一个关键帧的时间戳，避免seek后出现回退，解码出一样的图片！
				ffp_packet_queue_put(&demux->subq, pkt);
				ffp_packet_queue_put_nullpacket(&demux->subq, pkt->stream_index);
				demux->pktCount++;
			}
			else {
				//其他包释放内存忽略掉
				av_packet_unref(pkt);
			}
		}
	}
}

static int decode_interrupt_cb(void *ctx)
{
	IjkTextDemuxer *player = (IjkTextDemuxer*)ctx;
	return isAbortTextDemuxer(player);
}


AVFormatContext *openStream(IjkTextDemuxer* demux)
{
	bool error = false;
	AVFormatContext*formatCtx = avformat_alloc_context();
	if (!formatCtx) {
		error = true;
		goto ended;
	}

	formatCtx->interrupt_callback.callback = decode_interrupt_cb;
	formatCtx->interrupt_callback.opaque = demux;

	/*
	打开输入流，读取文件头信息，不会打开解码器；
	*/
	//低版本是 av_open_input_file 方法
	const char *moviePath = demux->contentPath;

	//打开文件流，读取头信息；
	if (0 != avformat_open_input(&formatCtx, moviePath, NULL, NULL)) {
		error = true;
		goto ended;
	}

	/* 刚才只是打开了文件，检测了下文件头而已，并不知道流信息；因此开始读包以获取流信息
	设置读包探测大小和最大时长，避免读太多的包！
	*/
	formatCtx->probesize = 1024 * 1024;
	formatCtx->max_analyze_duration = 10 * AV_TIME_BASE;

	if (0 != avformat_find_stream_info(formatCtx, NULL)) {
		error = true;
		goto ended;
	}

ended:
	//释放内存
	if (error) {
		if (formatCtx) {
			avformat_free_context(formatCtx);
		}

		return NULL;
	}
	else {
		return formatCtx;
	}
}

int findFirstSubStream(AVFormatContext *formatCtx)
{
	int first_sub_stream = -1;
	//查找H264格式的视频流
	for (int i = 0; i < formatCtx->nb_streams; i++) {
		AVStream *st = formatCtx->streams[i];
		enum AVMediaType type = st->codecpar->codec_type;
		//这里设置为了丢弃所有帧，解码器里会进行修改！
		st->discard = AVDISCARD_ALL;
		if (type == AVMEDIA_TYPE_SUBTITLE) {
			enum AVCodecID codec_id = st->codecpar->codec_id;
			if (codec_id == AV_CODEC_ID_TEXT) {
				first_sub_stream = i;
				break;
			}
		}
	}

	return first_sub_stream;
}

#define ITEM_NUM 5
void workFunc(IjkTextDemuxer* demux)
{
	char* error = NULL;
	AVFormatContext* formatCtx = openStream(demux);
	if (!formatCtx) {
		goto ended;
	}

	int sub_index = findFirstSubStream(formatCtx);
	if (sub_index < 0) {
		goto ended;
	}

	{
		AVStream *st = formatCtx->streams[sub_index];
		demux->ic = formatCtx;
		demux->st = st;
		demux->st_idx = sub_index;

		char metaKeyArr[ITEM_NUM][10] = { "title","album","artist","author","creator" };
		char *dumpDic[(ITEM_NUM+1)*2] = {NULL};

		for (int i = 0; i < ITEM_NUM; i++) {
			AVDictionaryEntry *entry = av_dict_get(formatCtx->metadata, metaKeyArr[i], NULL, 0);
			if (entry && entry->value) {
				dumpDic[2*i] = entry->key;
				dumpDic[2 * i + 1] = entry->value;
			}
		}

		if (demux->convertToAss) {
			//创建解码器可解码成 ASS 格式
			IjkSubDecoder *decoder = dumpStreamComponent(formatCtx, sub_index);
			if (decoder->codecName) {
				dumpDic[2*ITEM_NUM] = kMovieSubFmt;
				dumpDic[2 * ITEM_NUM + 1] = decoder->codecName;
			}

			if (!decoder) {
				goto ended;
			}

			if (ijk_openSubDecoder(decoder)) {
				demux->subDecoder = decoder;
				demux->subDecoder->name = "subDecoder";

				if (demux->onStreamOpenedBlock)
					demux->onStreamOpenedBlock(demux, dumpDic);

				ijk_startSubDecoder(demux->subDecoder);
			}
			else {
				//有的视频只有一个头，没有包也不能打开解码器；有的是编码格式不支持
				goto ended;
			}
		}
		else {
			if (demux->onStreamOpenedBlock)
				demux->onStreamOpenedBlock(demux, dumpDic);

			AVPacket pkt = { 0 };
			while (1) {
				int r = fetchAPacket(demux, &pkt);
				if (r <= 0) {
					break;
				}
				else {
					const char *cstr = (const char *)pkt.data;
					if (cstr && strlen(cstr) > 0) {
						char* str = av_strdup(cstr);
						//ctx的时间基不对，所以无法通过time_base计算
						float time_base = 1000;
						uint32_t pts = pkt.pts / time_base;

						if (demux->onFrameComingBlock)
							demux->onFrameComingBlock(demux, str, pts);
					}
				}
			}
		}
	}

ended:
	//释放内存
	if (formatCtx) {
		avformat_free_context(formatCtx);
	}

	free(demux->subDecoder);
	ffp_packet_queue_destroy(&demux->subq);
	//当取消掉时，不给上层回调
	if (!isAbortTextDemuxer(demux)) {
		if (demux->onFinishedBlock)
			demux->onFinishedBlock(demux, error);
	}
}

int fetchAPacket(IjkTextDemuxer* demux, AVPacket* pkt)
{
	int ret = -1;
	do {
		if (isAbortTextDemuxer(demux)) {
			return -1;
		}
		int r = ffp_packet_queue_get(&demux->subq, pkt, 0, NULL);

		if (r == 1) {
			ret = 1;
			break;
		}
		else if (r == 0 && !demux->readEOF) {
			//不能从队列里获取pkt，就去读取
			readPacketLoop(demux, demux->ic);
		}
		else {
			break;
		}
	} while (1);
	return ret;
}

int decoderWantAPacket(IjkTextDemuxer* demux, IjkSubDecoder* decoder,  AVPacket* pkt)
{
	if (decoder == demux->subDecoder) {
		return fetchAPacket(demux, pkt);
	}
	else {
		return -1;
	}
}

void decoderReceivedASub(IjkTextDemuxer* demux, IjkSubDecoder* decoder, AVSubtitle* sub)
{
	if (decoder == demux->subDecoder) {
		demux->frameCount++;

		const char *cstr = NULL;

		if (sub->num_rects > 0) {
			AVSubtitleRect* rect = sub->rects[0];
			if (rect->type == SUBTITLE_ASS) {
				cstr = rect->ass;
			}
			else if (rect->type == SUBTITLE_TEXT) {
				cstr = rect->text;
			}
		}

		if (cstr && strlen(cstr) > 0) {
			char *str = av_strdup(cstr);
			if (demux->onFrameComingBlock)
				demux->onFrameComingBlock(demux, str, sub->start_display_time);
		}
	}
}

bool decoderHasMorePacket(IjkTextDemuxer* demux)
{
	if (demux->subq.nb_packets > 0) {
		return true;
	}
	else {
		return !demux->readEOF;
	}
}


void Ijk_startTextDemuxer(IjkTextDemuxer* demux)
{
	assert(demux->contentPath);
	if (demux->workThread) {
		ALOGE("Not allowed to re create!");
	}

	//初始化视频包队列
	ffp_packet_queue_init(&demux->subq);
	ffp_packet_queue_start(&demux->subq);
	demux->workThread = SDL_CreateThreadEx(&demux->_workThread, workFunc, demux, "readPackets");
}

void Ijk_stopTextDemuxer(IjkTextDemuxer* demux)
{
	//主动stop时，则取消掉后续回调
	demux->onStreamOpenedBlock = NULL;
	demux->onFrameComingBlock = NULL;
	demux->onFinishedBlock = NULL;
	//仅仅标记为取消
	if (demux) {
		SDL_CancelThread(demux->workThread);
		ijk_cancelSubDecoder(demux->subDecoder);

		demux->subq.abort_request = 1;
	}
}