cmake_minimum_required(VERSION 3.16)

# 项目名称
project(MediaBrowserApp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Qt6 组件
find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent Core Gui)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 源文件列表
set(PROJECT_SOURCES
    main.cpp
    ThumbnailWorker.h
)

# 可执行文件
add_executable(MediaBrowserApp ${PROJECT_SOURCES})

# 链接库
tar_link_libraries(MediaBrowserApp PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::Concurrent
)

# 资源配置 (如果在 Windows 下需处理控制台)
if(WIN32)
    set_property(TARGET MediaBrowserApp PROPERTY WIN32_EXECUTABLE ON)
endif()
