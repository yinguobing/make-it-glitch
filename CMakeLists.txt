cmake_minimum_required(VERSION 3.0.0)
project(glitch VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 11)

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

add_executable(glitch src/main.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PkgConfig::LIBAV ${OpenCV_LIBS})
