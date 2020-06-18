//
// Created by chengkeke on 2020/6/13.
//

#ifndef FFMPEGPLAYERWITHQT_AVDECODE_H
#define FFMPEGPLAYERWITHQT_AVDECODE_H

#include "MediaPacket.h"
#include "MeidaStream.h"
#include "MediaFrame.h"
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
};


class AVDeCode {
public:
    AVDeCode();
    ~AVDeCode();

    int init(MeidaStream *stream);

    int sendPackt(MediaPacket *pkt);
    int receiveFrame(MediaFrame *frame);
private:
    AVCodecContext *m_avCodecContext = nullptr;
};


#endif //FFMPEGPLAYERWITHQT_AVDECODE_H
