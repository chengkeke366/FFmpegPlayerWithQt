//
// Created by chengkeke on 2020/6/13.
//

#ifndef FFMPEGPLAYERWITHQT_MEDIAPLAYER_H
#define FFMPEGPLAYERWITHQT_MEDIAPLAYER_H

extern "C"
{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
};

#include "MediaPacket.h"
#include "MeidaStream.h"

class MediaPlayer {
public:
    MediaPlayer();
    ~MediaPlayer();
    int openFile(const char * filename);
    int readPacket(MediaPacket *packet);
    int close();

    int getStreamCount();
    int getStream(MeidaStream *s, int streamId);
private:
    AVFormatContext * m_avFormatContext;
};


#endif //FFMPEGPLAYERWITHQT_MEDIAPLAYER_H
