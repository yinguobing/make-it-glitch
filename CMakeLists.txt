cmake_minimum_required(VERSION 3.0.0)
project(glitch VERSION 0.1.0)

option(WITH_GUI "Build with OpenCV highgui" ON)

configure_file(config.h.in config.h)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(
  LIBAV
  REQUIRED
  IMPORTED_TARGET
  libavdevice
  libavfilter
  libavformat
  libavcodec
  libswresample
  libswscale
  libavutil)

find_package(OpenCV REQUIRED)

add_executable(glitch src/main.cpp src/video_decoder.cpp)
target_include_directories(glitch PRIVATE ${PROJECT_BINARY_DIR} ${OpenCV_INCLUDE_DIRS})
target_link_libraries(glitch PkgConfig::LIBAV ${OpenCV_LIBS})

add_executable(essential src/essential.cpp)
target_include_directories(essential PRIVATE ${PROJECT_BINARY_DIR} ${OpenCV_INCLUDE_DIRS})
target_link_libraries(essential PkgConfig::LIBAV ${OpenCV_LIBS})
