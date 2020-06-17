//
// Created by chengkeke on 2020/6/13.
//

#include "AVDeCode.h"

AVDeCode::AVDeCode() {

    m_avCodecContext = avcodec_alloc_context3(nullptr);
}

AVDeCode::~AVDeCode() {
    if (m_avCodecContext)
    {
        avcodec_free_context(&m_avCodecContext);
        m_avCodecContext= nullptr;
    }
}
