//
// Created by chengkeke on 2020/6/13.
//
#include <stdio.h>
#include "MediaPlayer.h"

MediaPlayer::MediaPlayer() {
    av_register_all();
    m_avFormatContext  = avformat_alloc_context();
    if (m_avFormatContext == nullptr)
    {
        printf("avformat_alloc_context fail\n");
    }
}

MediaPlayer::~MediaPlayer() {
    if (m_avFormatContext != nullptr)
    {
        avformat_free_context(m_avFormatContext);
    }
}

int MediaPlayer::openFile(const char *filename) {
    //如果上一次打开文件后，未关闭，则第二次调用avformat_open_input 则会crash，所以再打开新文件之前，先关闭
    if (m_avFormatContext)
    {
        avformat_close_input(&m_avFormatContext);
    }
    auto ret = avformat_open_input(&m_avFormatContext,filename, nullptr, nullptr);
    if (ret != 0)
    {
        char buf[1024];
        av_strerror(ret, buf, 1024);
        printf("open input error\n");
        return -1;
    }else
    {
        printf("open input success\n");
        //获取流信息
        ret = avformat_find_stream_info(m_avFormatContext,nullptr);
        return  ret;
    }
}

int MediaPlayer::readPacket(MediaPacket *packet) {

    //读取packet
    int ret = av_read_frame(m_avFormatContext,packet->m_pakcet);
    return  ret;
}

int MediaPlayer::close() {
    if (m_avFormatContext == nullptr) {
        return -1;
    }
    avformat_close_input(&m_avFormatContext);
    return 0;
}

int MediaPlayer::getStreamCount() {
   int count = m_avFormatContext->nb_streams;
    return count;
}

int MediaPlayer::getStream(MeidaStream *s, int streamId) {
    AVStream *stream = m_avFormatContext->streams[streamId];

    return 0;
}
