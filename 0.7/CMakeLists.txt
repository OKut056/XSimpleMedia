cmake_minimum_required(VERSION 3.16)

project(MediaManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON) # 这行非常重要
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Widgets Multimedia MultimediaWidgets Concurrent REQUIRED)

# 包含头文件和源文件
add_executable(MediaManager
    main.cpp
    YouTubeStyleManager.h
    YouTubeStyleManager.cpp
    ThumbnailDelegate.h
    ThumbnailDelegate.cpp
    VideoDetailWidget.h
    VideoDetailWidget.cpp
    TagButton.h
    FfmpegUtil.h
    FfmpegUtil.cpp
    resources.qrc
)

target_link_libraries(MediaManager PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Concurrent
)
