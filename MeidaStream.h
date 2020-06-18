//
// Created by chengkeke on 2020/6/13.
//

#ifndef FFMPEGPLAYERWITHQT_MEIDASTREAM_H
#define FFMPEGPLAYERWITHQT_MEIDASTREAM_H

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
};
class MeidaStream {
public:

    AVCodecParameters *m_codecPar = nullptr;
    int      m_stream_index=-1;
};


#endif //FFMPEGPLAYERWITHQT_MEIDASTREAM_H
