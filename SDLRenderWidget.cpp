#include "SDLRenderWidget.h"


SDLRenderWidget::SDLRenderWidget(QWidget *parent /*= nullptr*/)
	:QWidget(parent)
{

	//setUpdatesEnabled(false);
	char winID[32] = { 0 };
	QSize size = this->baseSize();

	//B1. ��ʼ��SDL��ϵͳ��ȱʡ(�¼������ļ�IO���߳�)����Ƶ����Ƶ����ʱ��
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0)
	{
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
	}

	// B2. ����SDL���ڣ�SDL 2.0֧�ֶര��
	m_sdl_window = SDL_CreateWindowFrom((void*)(this->winId()));


	// B3. ����SDL_Renderer
	//     SDL_Renderer����Ⱦ��
	m_sdl_renderer = SDL_CreateRenderer(m_sdl_window, -1, 0);
}

SDLRenderWidget::~SDLRenderWidget()
{
	//���� window
	SDL_DestroyWindow(m_sdl_window);
	//�˳� SDL subsystems
	SDL_Quit();
}

void SDLRenderWidget::updateImage(std::shared_ptr<MediaFrame> yuv_frame)
{
	int nTextureWidth = 0, nTextureHeight = 0;
	//���Ȳ�ѯ��ǰ�������Ŀ�ߣ���������ϣ���ô��Ҫ�ؽ��������
	SDL_QueryTexture(m_sdl_texture, nullptr, nullptr, &nTextureWidth, &nTextureHeight);

	//B4 SDL_CreateTexture
	if (nTextureWidth != yuv_frame->m_frame->width || nTextureHeight != yuv_frame->m_frame->height) {
		if (m_sdl_texture)
			SDL_DestroyTexture(m_sdl_texture);
		//����ָ������Ⱦ�����ݸ�ʽ�����ʷ�ʽ�Ϳ�ߴ�С
		m_sdl_texture = SDL_CreateTexture(m_sdl_renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
			yuv_frame->m_frame->width, yuv_frame->m_frame->height);
	}

	//B5.ʹ���µ�YUV�������ݸ���SDL_Rect
	SDL_UpdateYUVTexture(m_sdl_texture,                   // sdl texture
		NULL,								// sdl rect
		yuv_frame->m_frame->data[0],            // y plane
		yuv_frame->m_frame->linesize[0],        // y pitch
		yuv_frame->m_frame->data[1],            // u plane
		yuv_frame->m_frame->linesize[1],        // u pitch
		yuv_frame->m_frame->data[2],            // v plane
		yuv_frame->m_frame->linesize[2]         // v pitch
	);


	// B6. ʹ���ض���ɫ��յ�ǰ��ȾĿ��
	SDL_RenderClear(m_sdl_renderer);

	// B7. ʹ�ò���ͼ������(texture)���µ�ǰ��ȾĿ��
	SDL_RenderCopy(m_sdl_renderer,                        // sdl renderer
		m_sdl_texture,									 // sdl texture
		NULL,											 // src rect, if NULL copy texture
		NULL											 // dst rect
	);

	// B8. ִ����Ⱦ��������Ļ��ʾ
	SDL_RenderPresent(m_sdl_renderer);
	
	// B9. ����֡��Ϊ25FPS���˴�����׼ȷ��δ���ǽ������ĵ�ʱ��
	//SDL_Delay(40);
}

SDL_AudioDeviceID SDLRenderWidget::openAudioDevice(SDL_AudioSpec *spec)
{
	SDL_AudioSpec  have;
	SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, spec, &have, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
	if (dev == 0) {
		SDL_Log("Failed to open audio: %s", SDL_GetError());
	}
	else {
		if (have.format != spec->format) { /* we let this one thing change. */
			SDL_Log("We didn't get Float32 audio format.");
		}
	}
	SDL_PauseAudioDevice(dev, 0);
	return dev;
}
