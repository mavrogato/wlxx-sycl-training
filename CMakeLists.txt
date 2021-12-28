
cmake_minimum_required(VERSION 3.14)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_COMPILER icx)
set(CMAKE_CXX_COMPILER icpx)
set(CMAKE_CXX_FLAGS_DEBUG "-g3")
set(CMAKE_CXX_FLAGS "-O0")

project(wlxx-sycl-training)

find_package(IntelDPCPP REQUIRED)
# find_package(OpenCL REQUIRED)

add_executable(wlxx-sycl-training
  main.cc)

target_compile_options(wlxx-sycl-training
  PRIVATE
  -std=c++20
  -stdlib=libstdc++
  -fcoroutines-ts)

target_link_libraries(wlxx-sycl-training
  PRIVATE
  c++
  OpenCL
  wayland-client
  wayland-egl
  GL
  EGL)

add_custom_target(run
  DEPENDS wlxx-sycl-training
  COMMAND wlxx-sycl-training)
