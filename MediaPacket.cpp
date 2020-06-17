//
// Created by chengkeke on 2020/6/13.
//

#include <stdio.h>
#include "MediaPacket.h"

MediaPacket::MediaPacket() {
    m_pakcet = av_packet_alloc();
    if (m_pakcet==nullptr)
    {
        printf("av_packet_alloc error\n");
    }else
    {
        printf("av_packet_alloc success\n");
    }
}

MediaPacket::~MediaPacket() {

    av_packet_free(&m_pakcet);
}
