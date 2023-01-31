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

int AVDeCode::init(MeidaStream *stream) {
    avcodec_parameters_to_context(m_avCodecContext,stream->m_codecPar);
    AVCodec *codec= avcodec_find_decoder(m_avCodecContext->codec_id);

	AVDictionary *opts = NULL;
	char refcount = 1;
	av_dict_set(&opts, "refcounted_frames", &refcount, 0);

    auto ret = avcodec_open2(m_avCodecContext,codec, &opts);
    if (ret)
    {
        printf("open codec error");
    }
    return 0;
}

int AVDeCode::sendPackt(MediaPacket *pkt) {
    int ret=0;
    if (pkt == nullptr)
    {
        ret = avcodec_send_packet(m_avCodecContext, nullptr);
    }else
    {
        ret = avcodec_send_packet(m_avCodecContext, pkt->m_pakcet);
    }
    return ret;
}

int AVDeCode::receiveFrame(MediaFrame *frame) {
    auto  ret = avcodec_receive_frame(m_avCodecContext, frame->m_frame);
    return ret;
}
