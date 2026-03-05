cmake_minimum_required(VERSION 4.2)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Undula)

find_package(OpenCV 4.12 REQUIRED)

set(SOURCES
  main.cpp
  util.cpp
  logging.cpp
  weights.cpp
  video.cpp
)

add_executable(undula ${SOURCES})

target_compile_options(undula PRIVATE -O3 -march=native)

target_include_directories(undula PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(undula
  PRIVATE
  ${OpenCV_LIBS}
)
