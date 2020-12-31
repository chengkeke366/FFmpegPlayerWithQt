#pragma once
#include <QWidget>

#include <memory>
#include "MediaFrame.h"
#include "SDL.h"


/* SDL_Thread Category£ºYou should not expect to be able to create a window, render, or receive events on any thread other than the main one.
*/
/*SDL_Render Category£ºThis API is not designed to be used from multiple threads¡£*/

class SDLRenderWidget  : public QWidget
{
	Q_OBJECT
public:
	SDLRenderWidget(QWidget *parent = nullptr);
	~SDLRenderWidget();
	void updateImage(std::shared_ptr<MediaFrame> frame);
	SDL_AudioDeviceID openAudioDevice(SDL_AudioSpec * spec);
private:
	SDL_Window *m_sdl_window = nullptr;
	SDL_Renderer*       m_sdl_renderer = nullptr;
	SDL_Texture*        m_sdl_texture = nullptr;
	SDL_Rect            m_sdl_rect;
};

