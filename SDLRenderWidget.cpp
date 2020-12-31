#include "SDLRenderWidget.h"


SDLRenderWidget::SDLRenderWidget(QWidget *parent /*= nullptr*/)
	:QWidget(parent)
{

	//setUpdatesEnabled(false);
	char winID[32] = { 0 };
	QSize size = this->baseSize();

	//B1. 初始化SDL子系统：缺省(事件处理、文件IO、线程)、视频、音频、定时器
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0)
	{
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
	}

	// B2. 创建SDL窗口，SDL 2.0支持多窗口
	m_sdl_window = SDL_CreateWindowFrom((void*)(this->winId()));


	// B3. 创建SDL_Renderer
	//     SDL_Renderer：渲染器
	m_sdl_renderer = SDL_CreateRenderer(m_sdl_window, -1, 0);
}

SDLRenderWidget::~SDLRenderWidget()
{
	//销毁 window
	SDL_DestroyWindow(m_sdl_window);
	//退出 SDL subsystems
	SDL_Quit();
}

void SDLRenderWidget::updateImage(std::shared_ptr<MediaFrame> yuv_frame)
{
	int nTextureWidth = 0, nTextureHeight = 0;
	//首先查询当前纹理对象的宽高，如果不符合，那么需要重建纹理对象
	SDL_QueryTexture(m_sdl_texture, nullptr, nullptr, &nTextureWidth, &nTextureHeight);

	//B4 SDL_CreateTexture
	if (nTextureWidth != yuv_frame->m_frame->width || nTextureHeight != yuv_frame->m_frame->height) {
		if (m_sdl_texture)
			SDL_DestroyTexture(m_sdl_texture);
		//这里指定了渲染的数据格式，访问方式和宽高大小
		m_sdl_texture = SDL_CreateTexture(m_sdl_renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
			yuv_frame->m_frame->width, yuv_frame->m_frame->height);
	}

	//B5.使用新的YUV像素数据更新SDL_Rect
	SDL_UpdateYUVTexture(m_sdl_texture,                   // sdl texture
		NULL,								// sdl rect
		yuv_frame->m_frame->data[0],            // y plane
		yuv_frame->m_frame->linesize[0],        // y pitch
		yuv_frame->m_frame->data[1],            // u plane
		yuv_frame->m_frame->linesize[1],        // u pitch
		yuv_frame->m_frame->data[2],            // v plane
		yuv_frame->m_frame->linesize[2]         // v pitch
	);


	// B6. 使用特定颜色清空当前渲染目标
	SDL_RenderClear(m_sdl_renderer);

	// B7. 使用部分图像数据(texture)更新当前渲染目标
	SDL_RenderCopy(m_sdl_renderer,                        // sdl renderer
		m_sdl_texture,									 // sdl texture
		NULL,											 // src rect, if NULL copy texture
		NULL											 // dst rect
	);

	// B8. 执行渲染，更新屏幕显示
	SDL_RenderPresent(m_sdl_renderer);
	
	// B9. 控制帧率为25FPS，此处不够准确，未考虑解码消耗的时间
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
