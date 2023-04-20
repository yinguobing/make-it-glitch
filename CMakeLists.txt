cmake_minimum_required(VERSION 3.0.0)
project(glitch VERSION 0.1.0)

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

add_executable(glitch src/main.cpp)
target_include_directories(glitch PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(glitch PkgConfig::LIBAV ${OpenCV_LIBS})

add_executable(export-glitch src/export.cpp)
target_include_directories(export-glitch PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(export-glitch PkgConfig::LIBAV ${OpenCV_LIBS})
