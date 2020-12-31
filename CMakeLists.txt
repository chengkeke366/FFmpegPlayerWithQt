cmake_minimum_required(VERSION 3.15)

project(FFmpegPlayerWithQt)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


set (FFMPEG_INCLUD ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg/include)
set (FFMPEG_LIB    ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg/lib)
set (SDL_INCLUDE  ${CMAKE_CURRENT_SOURCE_DIR}/SDL2/include)
set (SDL_LIB  ${CMAKE_CURRENT_SOURCE_DIR}/SDL2/lib/x86)

message("ffmpeg include  directories is ${FFMPEG_INCLUD}")
message("ffmpeg lib directories is ${FFMPEG_LIB}")
message("SDL include  directories is ${SDL_INCLUDE}")
message("SDL lib directories is ${SDL_LIB}")

include_directories(${FFMPEG_INCLUD} ${SDL_INCLUDE})
link_directories(${FFMPEG_LIB} ${SDL_LIB})


find_package(Qt5 COMPONENTS Widgets Core REQUIRED)

set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO  /NODEFAULTLIB:libc.lib")		# 程序输出文件为exe文件时起作用
set(CMAKE_SHARED_LINKR_FLAGS "${CMAKE_SHARED_LINKR_FLAGS} /SAFESEH:NO  /NODEFAULTLIB:libc.lib")		# 程序输出文件为dll文件时起作用
set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKR_FLAGS} /SAFESEH:NO  /NODEFAULTLIB:libc.lib")	        # 程序输出文件为lib文件时起作用
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /SAFESEH:NO /NODEFAULTLIB:libc.lib")

add_executable(FFmpegPlayerWithQt
		WIN32
        widget.ui
        widget.cpp
        widget.h
        main.cpp
        MediaPlayer.cpp MediaPlayer.h MediaPacket.cpp MediaPacket.h AVDeCode.cpp AVDeCode.h MeidaStream.cpp MeidaStream.h  MediaFrame.h
		SDLRenderWidget.cpp
		SDLRenderWidget.h
		)

target_link_libraries(FFmpegPlayerWithQt  Qt5::Widgets avformat avcodec avutil swscale SDL2 SDL2main.lib)

