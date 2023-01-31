//
// Created by chengkeke on 2020/6/13.
//
#include <stdio.h>
#include "MediaPlayer.h"
#include "MediaFrame.h"
#include <assert.h>

extern  "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>
}

#include <stdint.h>
#include <QMetaObject>
#include <QApplication>
#include <chrono>

#define PACKETSIZE     1024*5*10

#define AV_SYNC_THRESHOLD 0.01
#define AV_NOSYNC_THRESHOLD 10.0

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

// ...

std::string time_in_HH_MM_SS_MMM()
{
	using namespace std::chrono;

	// get current time
	auto now = system_clock::now();

	// get number of milliseconds for the current second
	// (remainder after division into seconds)
	auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

	// convert to std::time_t in order to convert to std::tm (broken time)
	auto timer = system_clock::to_time_t(now);

	// convert to broken time
	std::tm bt = *std::localtime(&timer);

	std::ostringstream oss;

	oss << std::put_time(&bt, "%H:%M:%S"); // HH:MM:SS
	oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

	return oss.str();
}
static short
format_convert(int format)
{
	short sdl_format = false;
	switch (format)
	{
	case AV_SAMPLE_FMT_U8:
	case AV_SAMPLE_FMT_U8P:
		sdl_format = AUDIO_U8;
		break;
	case AV_SAMPLE_FMT_S16:
	case AV_SAMPLE_FMT_S16P:
		sdl_format = AUDIO_S16SYS;
		break;
	case AV_SAMPLE_FMT_S32:
	case AV_SAMPLE_FMT_S32P:
		sdl_format = AUDIO_S32SYS;
		break;
	case AV_SAMPLE_FMT_FLT:
	case AV_SAMPLE_FMT_FLTP:
	case AV_SAMPLE_FMT_DBL:
	case AV_SAMPLE_FMT_DBLP:
		sdl_format = AUDIO_F32SYS;
		break;
	default:
		printf("err fmt!=%d\n", format);
		return false;
	}
	return sdl_format;
}


char*
av_pcm_clone(AVFrame* frame)
{
	assert(NULL != frame);

	int32_t bytes_per_sample = av_get_bytes_per_sample((enum AVSampleFormat)frame->format);
	char* p_cur_ptr = NULL, * pcm_data = NULL;
	if (bytes_per_sample <= 0)
		return NULL;


	// 1.For packet sample foramt and 1 channel,we just store pcm data in byte order.
	if ((1 == frame->channels)
		|| (frame->format >= AV_SAMPLE_FMT_U8 && frame->format <= AV_SAMPLE_FMT_DBL))
	{//linesize[0] maybe 0 or has padding bits,so calculate the real size by ourself.		
		int32_t frame_size = frame->channels * frame->nb_samples * bytes_per_sample;
		p_cur_ptr = pcm_data = (char*)malloc(frame_size);
		memcpy(p_cur_ptr, frame->data[0], frame_size);
	}
	else
	{//2.For plane sample foramt, we must store pcm datas interleaved. [LRLRLR...LR].
		int32_t frame_size = frame->channels * frame->nb_samples * bytes_per_sample;
		p_cur_ptr = pcm_data = (char*)malloc(frame_size);
		for (int i = 0; i < frame->nb_samples; ++i)//nb_samples 每个样本的采样数，左右声道各采样
		{
			memcpy(p_cur_ptr, frame->data[0] + i * bytes_per_sample, bytes_per_sample);
			p_cur_ptr += bytes_per_sample;
			memcpy(p_cur_ptr, frame->data[1] + i * bytes_per_sample, bytes_per_sample);
			p_cur_ptr += bytes_per_sample;
		}
	}
	return pcm_data;
}


MediaPlayer::MediaPlayer()
{

}

MediaPlayer::~MediaPlayer() {
	stop_play();
}

bool MediaPlayer::start_play(const char* file_name)
{
	stop_play();
	m_stop = false;
	printf("avformat_open_input file:%s\n", file_name);

	m_AVFormatContext = avformat_alloc_context();
	if (avformat_open_input(&m_AVFormatContext, file_name, NULL, NULL) < 0)
	{
		return false;
	}

	/* retrieve stream information */
	if (avformat_find_stream_info(m_AVFormatContext, NULL) < 0)
	{
		printf("Could not find stream information\n");
		return false;
	}

	auto openDecode = [this](AVFormatContext* formartContext, AVCodecContext** codecContext, enum AVMediaType type)
	{
		int stream_index = av_find_best_stream(formartContext, type, -1, -1, NULL, 0);
		AVStream* stream;
		if (type == AVMEDIA_TYPE_AUDIO)
		{
			m_audio_AVStream = stream = formartContext->streams[stream_index];
		}
		else if (type == AVMEDIA_TYPE_VIDEO)
		{
			m_video_AVStream = stream = formartContext->streams[stream_index];
		}

		AVCodec* dec = avcodec_find_decoder(stream->codecpar->codec_id);
		if (!dec) {
			fprintf(stderr, "Failed to find %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
			return AVERROR(EINVAL);
		}
		else
		{
			bool ret;
			*codecContext = avcodec_alloc_context3(dec);
			if ((avcodec_parameters_to_context(*codecContext, stream->codecpar)) < 0)
			{
				printf("Failed to copy %s codec parameters to decoder context\n");
			}
			AVDictionary *opts = NULL;
			//解码后的AVFrame归属调用者，否则属于解码器。属于解码器则在下一次调用解码函数时，则
			av_dict_set(&opts, "refcounted_frames", "1", 0);

			if ((ret = avcodec_open2(*codecContext, dec, &opts)) < 0) {
				printf("Failed to open %s codec\n");
			}
		}
	};

	//查找并打开解码器
	openDecode(m_AVFormatContext, &m_video_AVCodecContext, AVMEDIA_TYPE_VIDEO);
	openDecode(m_AVFormatContext, &m_audio_AVCodecContext, AVMEDIA_TYPE_AUDIO);
	//     初始化SWS context，用于后续图像转换
	//     此处第6个参数使用的是FFmpeg中的像素格式，对比参考注释B4
	//     FFmpeg中的像素格式AV_PIX_FMT_YUV420P对应SDL中的像素格式SDL_PIXELFORMAT_IYUV
	//     如果解码后得到图像的不被SDL支持，不进行图像转换的话，SDL是无法正常显示图像的
	//     如果解码后得到图像的能被SDL支持，则不必进行图像转换
	//     这里为了编码简便，统一转换为SDL支持的格式AV_PIX_FMT_YUV420P==>SDL_PIXELFORMAT_IYUV

	m_sws_ctx = sws_getContext(
		m_video_AVCodecContext->width,   // src width
		m_video_AVCodecContext->height,   // src height
		m_video_AVCodecContext->pix_fmt, // src format
		m_video_AVCodecContext->width,	 // dst width
		m_video_AVCodecContext->height,// dst height
		AV_PIX_FMT_YUV420P,				// dst format
		SWS_BICUBIC,					 // flags:使用何种算法进行转换。可参考https://blog.csdn.net/leixiaohua1020/article/details/12029505
		NULL,
		NULL,                  // dst filter
		NULL                   // param
	);

	SDL_AudioSpec spec;
	spec.freq = m_audio_AVCodecContext->sample_rate;
	spec.channels = (Uint8)m_audio_AVCodecContext->channels;
	spec.format = format_convert(m_audio_AVCodecContext->sample_fmt);
	spec.silence = (Uint8)0;
	spec.samples = m_audio_AVCodecContext->frame_size;
	spec.callback = NULL;
	m_currentAudioDeviceId = m_renderReceiveObj->openAudioDevice(&spec);

	m_theoretical_render_video_time = m_theoretical_render_audio_time = (double)av_gettime() / 1000000.0;//初始化时钟（获取当前系统时间，然后根据pts叠加到该时间上的方式进行同步）
	//启动线程
	m_demux_thread = std::thread(&MediaPlayer::demux_thread, this);
	m_audio_decode_thread = std::thread(&MediaPlayer::audio_decode_thread, this);
	m_video_decode_thread = std::thread(&MediaPlayer::video_decode_thread, this);
	m_video_render_thread = std::thread(&MediaPlayer::render_video_thread, this);
	m_audio_render_thread = std::thread(&MediaPlayer::render_audio_thread, this);
	return true;
}

bool MediaPlayer::stop_play()
{
	m_stop = true;
	m_playStatus = Stop;
	m_pause_condition_variable.notify_all();
	if (m_demux_thread.joinable())
	{
		m_demux_thread.join();
	}
	if (m_video_decode_thread.joinable())
	{
		m_pkt_vidoe_condition_variable.notify_all();
		m_video_decode_thread.join();
	}

	if (m_video_render_thread.joinable())
	{
		m_frame_video_condition_varible.notify_all();
		m_video_render_thread.join();
	}

	if (m_audio_decode_thread.joinable())
	{
		m_pkt_audio_condition_variable.notify_all();
		m_audio_decode_thread.join();
	}

	if (m_audio_render_thread.joinable())
	{
		m_audio_render_thread.join();
	}
	m_audio_frame_queue.clear();
	m_video_frame_queue.clear();
	m_video_frame_queue.clear();
	m_audio_packet_queue.clear();
	return true;
}

void MediaPlayer::pause_resume_play()
{
	if (PlayStatus::Pause == m_playStatus)
	{
		m_playStatus = PlayStatus::Playing;
		m_pause_condition_variable.notify_all();
		SDL_PauseAudioDevice(m_currentAudioDeviceId, 0);
	}

	else
	{
		m_playStatus = PlayStatus::Pause;
		SDL_PauseAudioDevice(m_currentAudioDeviceId, 1);
	}

}

void MediaPlayer::getVideoSize(int& width, int& height)
{
	if (m_video_AVCodecContext)
	{
		width = m_video_AVCodecContext->width;
		height = m_video_AVCodecContext->height;
	}
}

void MediaPlayer::registerRenderWindowsCallback(SDLRenderWidget* receiver)
{
	m_renderReceiveObj = receiver;
}

void MediaPlayer::demux_thread()
{
	auto pushPkt2Queue = [this](std::shared_ptr<MediaPacket> pkt)
	{
		auto codecId = m_AVFormatContext->streams[pkt->m_pakcet->stream_index]->codec->codec_type;
		if (codecId == AVMEDIA_TYPE_VIDEO)
		{
			/*
			time_t t = time(0);
			char ch[64];
			strftime(ch, sizeof(ch), "%Y-%m-%d %H-%M-%S", localtime(&t)); //年-月-日 时-分-秒
			printf("Time:%s【demux_thread】-----m_video_packet_queue.push(pkt);\n",ch);
			*/
			std::lock_guard<std::mutex> locker(m_pkt_video_queue_mutex);
			m_video_packet_queue.push_back(pkt);
			m_pkt_vidoe_condition_variable.notify_one();
		
		}
		else if (codecId == AVMEDIA_TYPE_AUDIO)
		{
			/*
			time_t t = time(0);
			char ch[64];
			strftime(ch, sizeof(ch), "%Y-%m-%d %H-%M-%S", localtime(&t)); //年-月-日 时-分-秒
			printf("【demux_thread】-----m_audio_packet_queue.push(pkt);\n",ch);
			*/
			std::lock_guard<std::mutex> locker(m_pkt_audio_queue_mutex);
			m_audio_packet_queue.push_back(pkt);
			m_pkt_audio_condition_variable.notify_one();
		}
	};

	while (!m_stop)
	{
		pauseOrResum();

		//这段需要确保音视频同步后，才能正常工作。否则由于packet不够导致无法正常解码播放
		printf("video packet queue size: %d, video frame queue size:%d, audio packet queue size:%d,audio frame queue size :%d\n",
			m_video_packet_queue.size(), m_video_frame_queue.size(), m_audio_packet_queue.size(), m_audio_frame_queue.size());
		
		{
			std::lock_guard <std::mutex>locker(m_pkt_video_queue_mutex);
			if (m_video_frame_queue.size() > 0)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}
		}

		{
			std::lock_guard <std::mutex>locker(m_pkt_audio_queue_mutex);
			if (m_audio_frame_queue.size() > 0)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}
		}
		
		std::shared_ptr <MediaPacket> packet = std::make_shared<MediaPacket>();
		int result = av_read_frame(m_AVFormatContext, packet->m_pakcet);
		if (result < 0)
		{
			if (result == AVERROR_EOF)
			{
				printf("file read end\n");
				break;
			}
		}
		pushPkt2Queue(packet);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	m_demux_finish = true;

	printf("[finished]:void MediaPlayer::demux_thread()\n");
}

void MediaPlayer::audio_decode_thread()
{

	//deal audio decode	
	auto decode = [this](std::shared_ptr<MediaPacket> audio_pkt) {
		int complete = 0;
		do
		{
			std::shared_ptr<MediaFrame> media_frame = std::make_shared<MediaFrame>();
			int ret = avcodec_decode_audio4(m_audio_AVCodecContext, media_frame->m_frame, &complete, audio_pkt->m_pakcet);
			if (ret < 0)
			{
				break;
			}
			if (complete)
			{
				std::unique_lock<std::mutex> locker(m_audio_frame_queue_mutex);
				m_audio_frame_queue.push_back(media_frame);
			}
			audio_pkt->m_pakcet->size = audio_pkt->m_pakcet->size - ret;
			audio_pkt->m_pakcet->data = audio_pkt->m_pakcet->data + ret;
		} while (audio_pkt->m_pakcet->size > 0);
	};

	while (!m_stop)
	{
		//deal pause operate
		pauseOrResum();
		std::shared_ptr<MediaPacket> audio_pkt;
		//get audio pkt frome queue
		{
			

			std::unique_lock<std::mutex> audio_que_loker(m_pkt_audio_queue_mutex);
			m_pkt_audio_condition_variable.wait(audio_que_loker, [this]() {
				if (!m_audio_packet_queue.empty())
				{
					return true;
				}
				else
				{
					if (m_demux_finish)
					{
						m_audio_decode_finish = true;
						return true;
					}
					else
					{
						return false;//block
					}

				}
			});
			//解码完成，退出解码线程
			if (m_audio_decode_finish)
			{
				//刷新解码器缓存
				std::shared_ptr<MediaPacket> FlushPkt = std::make_shared<MediaPacket>();
				FlushPkt->m_pakcet->data = NULL;
				FlushPkt->m_pakcet->size = 0;
				decode(FlushPkt);
				printf("audio_decodec thread finished..................\n");
				break;
			}
			audio_pkt = m_audio_packet_queue.front();
			m_audio_packet_queue.pop_front();
		}
		decode(audio_pkt);
	}
	

	printf("[finished]:void MediaPlayer::audio_decode_thread()\n");
}

void MediaPlayer::video_decode_thread()
{
	auto decode = [this](std::shared_ptr<MediaPacket> video_pkt) {
		//deal video decode
		int complete = 0;
		do
		{
			std::shared_ptr<MediaFrame> media_frame = std::make_shared<MediaFrame>();
			if (!media_frame->m_frame)
			{
				continue;
			}
			int ret = avcodec_decode_video2(m_video_AVCodecContext, media_frame->m_frame, &complete, video_pkt->m_pakcet);
			if (ret < 0)
			{
				break;//跳出内循环
			}
			else
			{
				if (complete)
				{
					std::lock_guard<std::mutex> locker(m_video_frame_queue_mutex);
					m_video_frame_queue.push_back(media_frame);
				}
				video_pkt->m_pakcet->data = video_pkt->m_pakcet->data + ret;
				video_pkt->m_pakcet->size = video_pkt->m_pakcet->size - ret;
			}
		} while (video_pkt->m_pakcet->size > 0);
	};

	while (!m_stop)
	{
		//deal pause operate
		pauseOrResum();
		std::shared_ptr<MediaPacket> video_pkt;
		{
			
			std::unique_lock<std::mutex> video_pkt_que_loker(m_pkt_video_queue_mutex);
			m_pkt_vidoe_condition_variable.wait(video_pkt_que_loker, [this]() {
				if (!m_video_packet_queue.empty())
				{
					return true;
				}
				else
				{
					if (m_demux_finish)
					{
						m_video_decode_finish = true;
						return true;
					}
					else
					{
						return false;
					}
				}
			});
			//视频解码完成
			if (m_video_decode_finish)
			{
				printf("video_decode_thread thread finished..................................\n");
				//flush 视频解码器缓存
				std::shared_ptr<MediaPacket> pkt = std::make_shared<MediaPacket>();
				pkt->m_pakcet->data = NULL;
				pkt->m_pakcet->size = 0;
				decode(pkt);
				break;
			}

			video_pkt = m_video_packet_queue.front();
			m_video_packet_queue.pop_front();
		}
		decode(video_pkt);
	}
	
	printf("[finished]:void MediaPlayer::video_decode_thread()\n");
}


void MediaPlayer::render_video_thread()
{
	int frameSize = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, m_video_AVCodecContext->width, m_video_AVCodecContext->height, 1);
	uint8_t* buffer = (uint8_t*)av_malloc(frameSize);//指向YUV420的数据部分

	static double m_previous_pts_diff = 40e-3;
	static double m_previous_pts = 0;
	static bool m_first_frame = true;
	double delay_until_next_wake;
	while (!m_stop)
	{
		bool late_first_frame = false;
		pauseOrResum();		
		if (m_video_decode_finish)
		{
			break;
		}

		std::unique_lock<std::mutex> locker(m_video_frame_queue_mutex);
		if (m_video_frame_queue.empty())
		{
			locker.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}
		std::shared_ptr<MediaFrame> video_frame = m_video_frame_queue.front();
		m_video_frame_queue.pop_front();
		locker.unlock();

		auto videoPts = video_frame->m_frame->best_effort_timestamp * av_q2d(m_video_AVStream->time_base);
		double pts_diff = videoPts - m_previous_pts;
		if (m_first_frame)
		{
			late_first_frame = pts_diff >= 1.0;
			m_first_frame = false;
		}
		if (pts_diff <= 0 || late_first_frame)
		{
			pts_diff = m_previous_pts_diff;
		}
		m_previous_pts_diff = pts_diff;
		m_previous_pts = videoPts;

		auto delay = av_gettime() - m_current_aduio_render_time;//渲染到现在为止的延迟
		int currentPts = m_audio_current_pts + delay;//当前音频的播放pts
		auto diff = videoPts - currentPts;//【计算视频帧pts与当前音频帧pts的差值，使用这个值来判断是否要展示当前的视频帧】
		double sync_threshold;

		//更新阈值
		sync_threshold = (pts_diff > AV_SYNC_THRESHOLD)
			? pts_diff : AV_SYNC_THRESHOLD;
		
		//修正pts_diff
		if (abs(diff)< AV_NOSYNC_THRESHOLD)
		{
			if (diff < - sync_threshold)//如果diff小于0（表明视频帧落后），且超出了阈值，则立即播放
			{
				pts_diff = 0;
			}else if (diff > sync_threshold)//diff大于阈值，则表明当前视频帧没有放完,则延迟两个pts_diff
			{
				pts_diff = 2 * pts_diff;
			}
		}
		m_theoretical_render_video_time += pts_diff;
	
		delay_until_next_wake = m_theoretical_render_video_time - (av_gettime() / 1000000.0L);
		if (delay_until_next_wake < 0.010L)
		{
			delay_until_next_wake = 0.010L;
		}

		printf("render video delay:%lf\n", delay_until_next_wake);
		auto wake_tp = std::chrono::high_resolution_clock::now()
			+ std::chrono::duration<int, std::micro>((int)(delay_until_next_wake * 1000000 +500));
		
		std::this_thread::sleep_until(wake_tp);


		std::shared_ptr<MediaFrame> yuv_frame = std::make_shared<MediaFrame>();	
		//将yuv_frame->m_frame->data数组指向buffer，并按照codecContext和AVPixelFormat决定data指针数组各个成员的指向
		av_image_fill_arrays(yuv_frame->m_frame->data,           // dst data[]
			yuv_frame->m_frame->linesize,						// dst linesize[]
			buffer,												// src buffer
			AV_PIX_FMT_YUV420P,									// pixel format
			m_video_AVCodecContext->width,						// width
			m_video_AVCodecContext->height,						// height
			1													// align
		);
		yuv_frame->m_frame->width = m_video_AVCodecContext->width;
		yuv_frame->m_frame->height = m_video_AVCodecContext->height;

		//sws_scale将各种video_format转换为AV_PIX_FMT_YUV420P	
		sws_scale(m_sws_ctx,					// sws context
			video_frame->m_frame->data,			// src slice
			video_frame->m_frame->linesize,		// src stride
			0,									// src slice y
			video_frame->m_frame->height,		// src slice height
			yuv_frame->m_frame->data,			// dst planes
			yuv_frame->m_frame->linesize);		// dst strides		
	
		static FILE * yuv = nullptr;
		if (yuv == nullptr)
		{
			yuv = fopen("C:/Users/ChengKeKe/Desktop/player420.yuv", "wb");
		}

// 		for (int j = 0; j < yuv_frame->m_frame->height; j++)//每个像素都保存一个Y，每行都采集
// 			fwrite(yuv_frame->m_frame->data[0] + j * yuv_frame->m_frame->linesize[0], yuv_frame->m_frame->width, 1, yuv);
// 		for (int j = 0; j < yuv_frame->m_frame->height / 2; j++)//隔行采集(height/2)，每两个Y采集一个U(width/2),
// 			fwrite(yuv_frame->m_frame->data[1] + j * yuv_frame->m_frame->linesize[1], yuv_frame->m_frame->width / 2, 1, yuv);
// 		for (int j = 0; j < yuv_frame->m_frame->height / 2; j++)//隔行采集(height/2)，每两个Y采集一个V((width/2))
// 			fwrite(yuv_frame->m_frame->data[2] + j * yuv_frame->m_frame->linesize[2], yuv_frame->m_frame->width / 2, 1, yuv);

		uint8_t *ypoint = yuv_frame->m_frame->data[0];
		for (int i=0; i< yuv_frame->m_frame->height; i++)
		{
			fwrite(ypoint,yuv_frame->m_frame->width,1,yuv) ;
			ypoint = ypoint + yuv_frame->m_frame->linesize[0];
		}
		uint8_t *upoint = yuv_frame->m_frame->data[1];
		for (int i = 0; i < yuv_frame->m_frame->height/2; i++)
		{
			fwrite(upoint, yuv_frame->m_frame->width/2, 1, yuv);
			upoint = upoint + yuv_frame->m_frame->linesize[1];
		}

		uint8_t *vpoint = yuv_frame->m_frame->data[2];
		for (int i = 0; i < yuv_frame->m_frame->height / 2; i++)
		{
			fwrite(vpoint, yuv_frame->m_frame->width / 2, 1, yuv);
			vpoint = vpoint + yuv_frame->m_frame->linesize[2];
		}
		fflush(yuv);
		//use QApplication::instance() raplace m_renderRceiveObj because it may invoke by  mutilthread. when it destruct will crash
		QMetaObject::invokeMethod(QApplication::instance(), [=]() {
			m_renderReceiveObj->updateImage(yuv_frame);
			}, Qt::QueuedConnection);
	
	}
	av_free(buffer);
	printf("[finished]:void MediaPlayer::render_video_thread()\n");
}

void MediaPlayer::render_audio_thread()
{
	while (!m_stop)
	{
		pauseOrResum();
		bool late_first_frame = false;
		std::unique_lock<std::mutex> locker(m_audio_frame_queue_mutex);
		static double m_previous_pts_diff = 40e-3;
		static bool m_first_frame = true;
		if (!m_audio_frame_queue.empty())
		{
			std::shared_ptr<MediaFrame> audio_frame = m_audio_frame_queue.front();
			m_audio_frame_queue.pop_front();
			locker.unlock();

			m_audio_current_pts = audio_frame->m_frame->best_effort_timestamp * av_q2d(m_audio_AVStream->time_base);
			// the amount of time until we need to display this frame
			double diff = m_audio_current_pts - m_previous_audio_pts;
			if (m_first_frame)
			{
				late_first_frame = diff >= 1.0;
				m_first_frame = false;
			}

			if (diff <= 0 || late_first_frame)
			{// if diff is invalid, use previous
				diff = m_previous_pts_diff;
			}

			// save for next time
			m_previous_audio_pts = m_audio_current_pts;
			m_previous_pts_diff = diff;

			m_theoretical_render_audio_time += diff;//理论上应该渲染的time_point
			double timeDiff = m_theoretical_render_audio_time - (av_gettime() / 1000000.0L);

			if (timeDiff < 0.010L)
			{
				timeDiff = 0.01L;
			}

			if (timeDiff > diff)
			{
				timeDiff = diff;
			}

			//printf("audio frame will delay:%lf \n", timeDiff);
			auto wake_tp = std::chrono::high_resolution_clock::now()
				+ std::chrono::duration<int, std::micro>((int)(timeDiff * 1000000));

			std::this_thread::sleep_until(wake_tp);

			int32_t bytes_per_sample = av_get_bytes_per_sample((enum AVSampleFormat)audio_frame->m_frame->format);
			int32_t size = audio_frame->m_frame->nb_samples * bytes_per_sample * audio_frame->m_frame->channels;
			char* data = av_pcm_clone(audio_frame->m_frame);
			m_current_aduio_render_time = av_gettime();
			SDL_QueueAudio(m_currentAudioDeviceId, data, size);
			free((void*)data);
		}
		else
		{
			locker.unlock();
			//printf("audio frame_queue is empty\n");
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		if (m_audio_decode_finish)
		{
			break;
		}		
	}
	printf("[finished]:void MediaPlayer::render_audio_thread()\n");
}

void MediaPlayer::pauseOrResum()
{
	std::unique_lock<std::mutex> locker(m_pause_mutex);
	m_pause_condition_variable.wait(locker, [this]() {
		if (PlayStatus::Pause == m_playStatus)
		{
			printf("demux_thread m_pause_condition_variable PlayStatus::Pause\n");
			return false;//阻塞线程
		}
		else
		{
			return true;//通过notify唤醒wait时，需要再次执行lamada，当lamada返回true才可以继续执行，否则，继续阻塞
		}
		});
}

