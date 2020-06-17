//
// Created by chengkeke on 2020/6/13.
//

#ifndef FFMPEGPLAYERWITHQT_AVDECODE_H
#define FFMPEGPLAYERWITHQT_AVDECODE_H

extern "C"
{
#include <libavcodec/avcodec.h>
};


class AVDeCode {
public:
    AVDeCode();
    ~AVDeCode();

    //int init()

private:
    AVCodecContext *m_avCodecContext = nullptr;
};


#endif //FFMPEGPLAYERWITHQT_AVDECODE_H
