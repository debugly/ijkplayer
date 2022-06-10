//
//  ijk_sub_decoder.c
//

#include "ijk_sub_decoder.h"
#include "ijk_text_demuxer.h"
#include "ijkplayer\ijkplayer.h"

IjkSubDecoder* ijk_createSubDecoder()
{
	IjkSubDecoder* dec = (IjkSubDecoder*)calloc(sizeof(IjkSubDecoder), 1);
	dec->streamIdx = -1;
	dec->codecID = -1;

	return dec;
}

void ijk_dumpStreamFormat(IjkSubDecoder* dec)
{
	if (dec->ic == NULL) {
		return;
	}

	if (dec->streamIdx < 0 || dec->streamIdx >= dec->ic->nb_streams) {
		return;
	}

	AVStream *stream = dec->ic->streams[dec->streamIdx];

	if (stream->codecpar) {
		//解码器id
		enum AVCodecID codecID = stream->codecpar->codec_id;
		dec->codecID = codecID;
	}
}

char* ijk_getCodecName(IjkSubDecoder* dec)
{
	if (dec->codecID == -1) {
		return NULL;
	}

	//根据解码器id找到对应名称
	const char *codecName = avcodec_get_name(dec->codecID);
	return codecName;
}

bool ijk_openSubDecoder(IjkSubDecoder* dec)
{
	if (dec->ic == NULL) {
		return false;
	}

	if (dec->streamIdx < 0 || dec->streamIdx >= dec->ic->nb_streams) {
		return false;
	}

	AVStream *stream = dec->ic->streams[dec->streamIdx];

	//创建解码器上下文
	AVCodecContext *avctx = avcodec_alloc_context3(NULL);
	if (!avctx) {
		return false;
	}

	//填充下相关参数
	if (avcodec_parameters_to_context(avctx, stream->codecpar)) {
		avcodec_free_context(&avctx);
		return false;
	}

	//av_codec_set_pkt_timebase(avctx, stream->time_base);

	//查找解码器
	AVCodec *codec = avcodec_find_decoder(avctx->codec_id);
	if (!codec) {
		avcodec_free_context(&avctx);
		return false;
	}

	avctx->codec_id = codec->id;
	// important! 前面设置成了 AVDISCARD_ALL，这里必须修改下，否则可能导致读包失败；根据vtp的场景，这里使用丢弃非关键帧比较合适
	stream->discard = AVDISCARD_DEFAULT;
	//打开解码器
	if (avcodec_open2(avctx, codec, NULL)) {
		avcodec_free_context(&avctx);
		return false;
	}
	dec->stream = stream;
	dec->avctx = avctx;
	dec->startTime = stream->start_time;
	return false;
}

#pragma mark - 音视频通用解码方法
int decodeAFrame(IjkSubDecoder* dec, AVSubtitle* sub)
{
	int ret;
	AVPacket pkt;

	//[阻塞等待]直到获取一个packet
	int r = -1;
	r = decoderWantAPacket(dec->delegDemuxer, dec, &pkt);
	if (r < 0){
		return -1;
	}
	int got_frame = 0;

	ret = avcodec_decode_subtitle2(dec->avctx, sub, &got_frame, &pkt);

	if (got_frame) {
		uint32_t pts = 0;
		if (pkt.pts != AV_NOPTS_VALUE) {
			int codec_id = dec->codecID;
			//ctx的时间基不对，所以无法通过time_base计算
			float time_base = 1;
			if (codec_id == AV_CODEC_ID_ASS) {
				time_base = 100;
			}
			else if (codec_id == AV_CODEC_ID_SUBRIP
				|| codec_id == AV_CODEC_ID_WEBVTT
				|| codec_id == AV_CODEC_ID_TEXT) {
				time_base = 1000;
			}
			//其它格式pts为0，可暴露问题
			pts = pkt.pts / time_base;
			//use packet's pts as start_display_time
			sub->start_display_time = pts;
		}

		decoderReceivedASub(dec->delegDemuxer, dec, sub);
	}

	//释放内存
	av_packet_unref(&pkt);
	return ret;
}

#pragma mark - 解码开始
void ijk_startSubDecoder(IjkSubDecoder* dec)
{
	AVSubtitle sub = { 0 };

	do {
		if (dec->abort_request) {
			av_log(NULL, AV_LOG_ERROR, "%s cancel.\n", dec->name);
			break;
		}
		//使用通用方法解码一帧
		int got_frame = decodeAFrame(dec, &sub);

		if (dec->abort_request) {
			av_log(NULL, AV_LOG_ERROR, "%s cancel.\n", dec->name);
			break;
		}

		//解码出错
		if (got_frame < 0) {
			bool hasMorePkt = false;
			hasMorePkt = decoderHasMorePacket(dec->delegDemuxer);

			if (hasMorePkt) {
				av_log(NULL, AV_LOG_DEBUG, "has more pkt need decode.\n");
				continue;
			}
			else {
				av_log(NULL, AV_LOG_ERROR, "%s eof.\n", dec->name);
				break;
			}
		}
		else {
			decoderReceivedASub(dec->delegDemuxer, dec, &sub);
		}
	} while (1);
}

void ijk_cancelSubDecoder(IjkSubDecoder* dec)
{
	dec->abort_request = true;
}
