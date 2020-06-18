cmake_minimum_required(VERSION 3.15)

project(FFmpegPlayerWithQt)


set(CMAKE_CXX_STANDARD 11)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


set (FFMPEG_INCLUDE /usr/local/Cellar/ffmpeg/3.4.2/include)
set (FFMPEG_LIB /usr/local/Cellar/ffmpeg/3.4.2/lib)
message("ffmpeg include  directories is ${FFMPEG_INCLUDE}")
message("ffmpeg lib directories is ${FFMPEG_LIB}")

include_directories(${FFMPEG_INCLUDE})
link_directories(${FFMPEG_LIB})


find_package(Qt5 COMPONENTS Widgets Core REQUIRED)

add_executable(FFmpegPlayerWithQt
        widget.ui
        widget.cpp
        widget.h
        main.cpp
        MediaPlayer.cpp MediaPlayer.h MediaPacket.cpp MediaPacket.h AVDeCode.cpp AVDeCode.h MeidaStream.cpp MeidaStream.h MediaFrame.cpp MediaFrame.h)

target_link_libraries(FFmpegPlayerWithQt Qt5::Widgets avformat)


