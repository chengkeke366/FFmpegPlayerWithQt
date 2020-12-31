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
#include "MediaFrame.h"
#include <thread>
#include <condition_variable>
#include <mutex>
#include <deque>
#include <vector>
#include <list>
#include <QObject>
#include "SDLRenderWidget.h"

struct SwsContext;

class MediaPlayer:public QObject
{
	Q_OBJECT
public:
    MediaPlayer();
    ~MediaPlayer();
	enum PlayStatus
	{
		None,//打开播放器时的状态
		Ready,//加载文件之后
		Playing,//播放中
		Pause,//暂停
		Stop,//停止
	};
	bool start_play(const char * file_name);
	bool stop_play();
	void pause_resume_play();
	void getVideoSize(int &width, int &height);
public:
	void registerRenderWindowsCallback(SDLRenderWidget *receiver);
private:
	void pauseOrResum();

	void demux_thread();
	void audio_decode_thread();
	void video_decode_thread();
	void render_video_thread();
	void render_audio_thread();

	static void sdlAudioCallBackFunc(void *userdata, Uint8 *stream, int len);
signals:
	void readReady(std::shared_ptr<MediaFrame> frame);
private:
    AVFormatContext * m_AVFormatContext;
	AVCodecContext  * m_video_AVCodecContext;
	AVStream        * m_video_AVStream;
	AVCodecContext  * m_audio_AVCodecContext;
	AVStream        * m_audio_AVStream;
	struct SwsContext*  m_sws_ctx = NULL;

	std::thread  m_demux_thread;
	std::thread  m_video_decode_thread;
	std::thread  m_audio_decode_thread;
	std::thread  m_video_render_thread;
	std::thread  m_audio_render_thread;
	

	std::deque<std::shared_ptr<MediaPacket>> m_video_packet_queue;
	std::deque<std::shared_ptr<MediaPacket>> m_audio_packet_queue;
	std::deque<std::shared_ptr<MediaFrame>> m_audio_frame_queue;
	std::deque<std::shared_ptr<MediaFrame>> m_video_frame_queue;
	
	std::mutex   m_pkt_audio_queue_mutex;
	std::mutex   m_pkt_video_queue_mutex;
	std::mutex   m_audio_frame_queue_mutex;
	std::mutex	 m_video_frame_queue_mutex;
	std::mutex   m_pause_mutex;
	
	std::function<void(std::shared_ptr<MediaFrame>)> m_video_render_callback_fuc;
	SDLRenderWidget                                  *m_renderReceiveObj = nullptr;
	SDL_AudioDeviceID                                m_currentAudioDeviceId;

	std::condition_variable m_pause_condition_variable;
	std::condition_variable m_demuex_condition_variable;
	std::condition_variable m_pkt_audio_condition_variable;
	std::condition_variable m_pkt_vidoe_condition_variable;
	std::condition_variable m_frame_audio_condition_varible;
	std::condition_variable m_frame_video_condition_varible;
	
	bool m_demux_finish = false;
	bool m_audio_decode_finish = false;
	bool m_video_decode_finish = false;

	bool m_stop = false;
	PlayStatus m_playStatus = None;
	double m_will_render_audio_time;
	double m_previous_pts_diff = 40e-3;
	bool m_first_frame = true; 
};


#endif //FFMPEGPLAYERWITHQT_MEDIAPLAYER_H
