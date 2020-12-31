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

			if ((ret = avcodec_open2(*codecContext, dec, NULL)) < 0) {
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

	m_will_render_audio_time = (double)av_gettime() / 1000000.0;
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
		m_frame_audio_condition_varible.notify_all();
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
			printf("【demux_thread】vidoe_condition_variable.notify_one\n");
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
			printf("【demux_thread】audio_condition_variable.notify_one\n");
			m_pkt_audio_condition_variable.notify_one();
		}
	};

	while (!m_stop)
	{
		pauseOrResum();

		{
			std::lock_guard <std::mutex>locker(m_pkt_video_queue_mutex);
			if (m_video_packet_queue.size() > 5)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}
		}
		{
			std::lock_guard <std::mutex>locker(m_pkt_audio_queue_mutex);
			if (m_audio_packet_queue.size() > 5)
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
		/*
			time_t t = time(0);
			char ch[64];
			strftime(ch, sizeof(ch), "%Y-%m-%d %H-%M-%S", localtime(&t)); //年-月-日 时-分-秒
			printf("Time:%s av_read_frame , the pkt pts is :%lld\n",ch, packet->m_pakcet->pts);
			*/
		pushPkt2Queue(packet);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	m_demux_finish = true;

	printf("[finished]:void MediaPlayer::demux_thread()\n");
}

void MediaPlayer::audio_decode_thread()
{
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
				printf("audio_decodec thread finished..................\n");
				break;
			}

			audio_pkt = m_audio_packet_queue.front();
			m_audio_packet_queue.pop_front();
		}

		//deal audio decode
			
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
				/*
				time_t t = time(0);
				char ch[64];
				strftime(ch, sizeof(ch), "%Y-%m-%d %H-%M-%S", localtime(&t)); //年-月-日 时-分-秒
				printf("Time:%s【audio_decode_thread】avcodec_decode_audio4 ,m_frame pts is:%lld, pkt_pts:%lld\n", ch, media_frame->m_frame->pts, media_frame->m_frame->pkt_pts);
				*/
				std::lock_guard<std::mutex> locker(m_audio_frame_queue_mutex);
				m_audio_frame_queue.push_back(media_frame);
				printf("【audio_decode_thread】m_frame_audio_condition_varible notify_one\n");
				m_frame_audio_condition_varible.notify_one();

				int size = av_get_bytes_per_sample((AVSampleFormat)media_frame->m_frame->format);
				static FILE *fd = nullptr;
				if (fd == nullptr)
				{
					fd = fopen("D:/origin.PCM","wb");
				}

				if (fd)
				{
					for (int i = 0; i < media_frame->m_frame->nb_samples; i++)
					{
						for (int ch = 0; ch < media_frame->m_frame->channels; ch++)
						{
							fwrite(media_frame->m_frame->data[ch] + size*i, 1, size, fd);
							//fflush(fd);
						}
					}
				}
			}
			audio_pkt->m_pakcet->size = audio_pkt->m_pakcet->size - ret;
			audio_pkt->m_pakcet->data = audio_pkt->m_pakcet->data + ret;
		} while (audio_pkt->m_pakcet->size > 0);

		
	}
	printf("[finished]:void MediaPlayer::audio_decode_thread()\n");
}

void MediaPlayer::video_decode_thread()
{
	while (!m_stop)
	{
		//deal pause operate
		pauseOrResum();
		printf("【video_decode_thread】after pauseOrResum\n");
		std::shared_ptr<MediaPacket> video_pkt;
		//get video pkt frome queue
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
				break;
			}
			video_pkt = m_video_packet_queue.front();
			m_video_packet_queue.pop_front();
		}

		//deal video decode
		int complete = 0;
		do
		{
			std::shared_ptr<MediaFrame> media_frame = std::make_shared<MediaFrame>();
			int ret = avcodec_decode_video2(m_video_AVCodecContext, media_frame->m_frame, &complete, video_pkt->m_pakcet);
			if (ret < 0)
			{
				break;//跳出内循环
			}
			else
			{
				if (complete)
				{
					/*
					time_t t = time(0);
					char ch[64];
					strftime(ch, sizeof(ch), "%Y-%m-%d %H-%M-%S", localtime(&t)); //年-月-日 时-分-秒
					printf("Time:%s【video_decode_thread】avcodec_decode_video2 ,m_frame pts is:%lld, pkt_pts:%lld\n", ch, media_frame->m_frame->pts, media_frame->m_frame->pkt_pts);
					*/
					std::lock_guard<std::mutex> locker(m_video_frame_queue_mutex);
					m_video_frame_queue.push_back(media_frame);
					m_frame_video_condition_varible.notify_one();
					printf("【video_decode_thread】m_frame_video_condition_varible.notify_one()\n");
				}
				video_pkt->m_pakcet->data = video_pkt->m_pakcet->data + ret;
				video_pkt->m_pakcet->size = video_pkt->m_pakcet->size - ret;
			}
		} while (video_pkt->m_pakcet->size > 0);
	}

	printf("[finished]:void MediaPlayer::video_decode_thread()\n");
}


void MediaPlayer::render_video_thread()
{
	int frameSize = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, m_video_AVCodecContext->width, m_video_AVCodecContext->height, 1);
	uint8_t* buffer = (uint8_t*)av_malloc(frameSize);//指向YUV420的数据部分
	while (!m_stop)
	{
		pauseOrResum();
		printf("【render_video_thread】after pauseOrResum\n");

		std::shared_ptr<MediaFrame> video_frame;
		{
			std::unique_lock<std::mutex> locker(m_video_frame_queue_mutex);
			m_frame_video_condition_varible.wait(locker, [this]() {
				if (!m_video_frame_queue.empty())
				{
					return true;
				}
				else
				{
					return m_video_decode_finish;
				}
				});
			if (m_video_decode_finish)
			{
				break;
			}
			video_frame = m_video_frame_queue.front();
			m_video_frame_queue.pop_front();
		}

		printf("【render_video_thread】after m_frame_audio_condition_varible wait\n");
		/*
		time_t t = time(0);
		char ch[64];
		strftime(ch, sizeof(ch), "%Y-%m-%d %H-%M-%S", localtime(&t)); //年-月-日 时-分-秒
		printf("Time:%s【render_thread】-----m_video_frame_queue.pop\n", ch);
		*/

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
		printf("【render_video_thread】invokeMethod\n");
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
		printf("【render_audio_thread】after pauseOrResum\n");
		bool late_first_frame = false;
		//audio
		std::shared_ptr<MediaFrame> audio_frame;
		{
			std::unique_lock<std::mutex> locker(m_audio_frame_queue_mutex);
			m_frame_audio_condition_varible.wait(locker, [this]() {
				if (!m_audio_frame_queue.empty())
				{
					return true;
				}
				else
				{
					return m_audio_decode_finish;
				}
				});
			if (m_audio_decode_finish)
			{
				break;
			}
			audio_frame = m_audio_frame_queue.front();
			m_audio_frame_queue.pop_front();	
		}
		printf("【render_audio_thread】after m_frame_audio_condition_varible wait\n");

		int32_t bytes_per_sample = av_get_bytes_per_sample((enum AVSampleFormat)audio_frame->m_frame->format);
		int32_t size = audio_frame->m_frame->nb_samples * bytes_per_sample * audio_frame->m_frame->channels;

		static FILE* g_file = nullptr;

		if (g_file == nullptr)
		{
			g_file = fopen("D:/test.pcm", "wb");
		}
		char* data = av_pcm_clone(audio_frame->m_frame);
		if (g_file)
		{
			fwrite(data, size, 1, g_file);
		}
		printf("【render_audio_thread】\n");
		SDL_QueueAudio(m_currentAudioDeviceId, data, size);
		free((void*)data);


		static double m_previous_pts = 0;
		double pts = audio_frame->m_frame->best_effort_timestamp * av_q2d(m_audio_AVStream->time_base);
		double diff = pts - m_previous_pts;

		//printf("audio pts is:%lf, audio with previous frame pts diff is:%lf \n", pts, diff);

		static auto prevoiusTime = std::chrono::system_clock::now();
		auto currentTime = std::chrono::system_clock::now();

		std::chrono::milliseconds mill = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - prevoiusTime);
		prevoiusTime = currentTime;
		//printf("system time is:%s, system time with previous time is:%lld\n", time_in_HH_MM_SS_MMM().c_str(), mill);
		if (m_first_frame)
		{
			late_first_frame = diff >= 1.0;
			m_first_frame = false;
		}

		if (diff <= 0 || late_first_frame)
		{
			diff = m_previous_pts_diff;
		}

		m_previous_pts = pts;
		m_previous_pts_diff = diff;

		m_will_render_audio_time += diff;//理论上应该渲染的time_point
		double timeDiff = m_will_render_audio_time - (av_gettime() / 1000000.0L);

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
	}
	printf("[finished]:void MediaPlayer::render_audio_thread()\n");
}


void MediaPlayer::sdlAudioCallBackFunc(void* userdata, Uint8* stream, int len)
{
	MediaPlayer* obj = static_cast<MediaPlayer*>(userdata);
	int len1, audio_data_size;
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

