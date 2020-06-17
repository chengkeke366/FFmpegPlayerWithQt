//
// Created by chengkeke on 2020/6/13.
//

#ifndef FFMPEGPLAYERWITHQT_MEDIAPACKET_H
#define FFMPEGPLAYERWITHQT_MEDIAPACKET_H

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
};

class MediaPacket {
public:
    MediaPacket();
    ~MediaPacket();

    AVPacket *m_pakcet;
};


#endif //FFMPEGPLAYERWITHQT_MEDIAPACKET_H
