//
//  ff_packet_list.h
//  IJKMediaPlayerKit
//
//  Created by Reach Matt on 2022/5/16.
//

#ifndef ff_packet_list_h
#define ff_packet_list_h

#include "ff_ffplay_def.h"

int packet_queue_put_private(PacketQueue *q, AVPacket *pkt);
int packet_queue_put(PacketQueue *q, AVPacket *pkt);
int packet_queue_put_nullpacket(PacketQueue *q, int stream_index);
/* packet queue handling */
int packet_queue_init(PacketQueue *q);
void packet_queue_flush(PacketQueue *q);
void packet_queue_destroy(PacketQueue *q);
void packet_queue_abort(PacketQueue *q);
void packet_queue_start(PacketQueue *q);
int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial);

#endif /* ff_packet_list_h */
