//
// Created by chengkeke on 2020/6/18.
//

#ifndef FFMPEGPLAYERWITHQT_MEDIAFRAME_H
#define FFMPEGPLAYERWITHQT_MEDIAFRAME_H

extern  "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
};

class MediaFrame {
public:
    AVFrame *m_frame;
};


#endif //FFMPEGPLAYERWITHQT_MEDIAFRAME_H