cmake_minimum_required(VERSION 3.18)

project(mandelbrot_cuda LANGUAGES CXX CUDA)

find_package(CUDAToolkit REQUIRED)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

add_executable(mandelbrot_cuda
    src/main.cpp
    src/mandelbrot.cu
    src/image_utils.cpp
    src/video_utils.cpp
)

target_include_directories(mandelbrot_cuda PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(mandelbrot_cuda PRIVATE CUDA::cudart)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mandelbrot_cuda PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-O3>)
endif()

target_compile_options(mandelbrot_cuda PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-O3>)

set_target_properties(mandelbrot_cuda PROPERTIES
    CUDA_SEPARABLE_COMPILATION OFF
)
