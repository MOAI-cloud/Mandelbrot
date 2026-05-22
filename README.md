# Mandelbrot CUDA

A CUDA/CMake project that renders a Mandelbrot set on the GPU, saves PNG images with `stb_image_write.h`, and can stream zoom animations to MP4 through `ffmpeg`. The renderer uses smooth escape-time coloring with selectable perceptual colormaps.

## Requirements

- NVIDIA GPU with a working CUDA driver
- CUDA Toolkit with `nvcc`
- CMake 3.18 or newer
- A C++17-capable host compiler
- `ffmpeg` for MP4 video output

## Build

From the project root:

```sh
cmake -S . -B build
cmake --build build -j
```

## Run

```sh
./build/mandelbrot_cuda --output output/mandelbrot.png
```

Default render settings are 1920x1080, 1000 iterations, centered near the classic Mandelbrot view.

Available options:

```txt
--width <pixels>        Image width, default 1920
--height <pixels>       Image height, default 1080
--iterations <count>    Maximum iterations per pixel, default 1000
--center-x <value>      View center on the real axis, default -0.75
--center-y <value>      View center on the imaginary axis, default 0.0
--scale <value>         Vertical view size, default 3.0
--output <path>         Output PNG path, default output/mandelbrot.png
--colormap <name>       Colormap: magma, inferno, viridis, cividis, turbo, classic; default magma
--no-image              Skip PNG output when rendering video
--video <path>          Output MP4 path and enable video rendering
--frames <count>        Video frame count, default 180 when video is enabled
--fps <count>           Video frames per second, default 30
--video-encoder <name>  ffmpeg encoder: libx264, h264_nvenc, hevc_nvenc; default libx264
--end-scale <value>     Final video scale, default 2% of --scale
--help                  Show usage
```

Example zoom:

```sh
./build/mandelbrot_cuda --width 2560 --height 1440 --iterations 2000 --center-x -0.743643887037151 --center-y 0.13182590420533 --scale 0.002 --colormap inferno --output output/zoom.png
```

Example zoom video:

```sh
./build/mandelbrot_cuda --width 1920 --height 1080 --iterations 1500 --center-x -0.743643887037151 --center-y 0.13182590420533 --scale 0.01 --end-scale 0.00002 --frames 240 --fps 60 --colormap magma --no-image --video output/zoom.mp4
```

If your ffmpeg build supports NVIDIA hardware encoding, add `--video-encoder h264_nvenc` for faster MP4 encoding. `libx264` remains the default because it is widely available and produces reliable output on machines without NVENC.

The renderer reuses pinned host memory and a single device buffer for still images and videos. The CUDA kernel uses precomputed coordinate steps, 32x8 thread blocks, vectorized RGBA stores, smooth escape-time coloring, and Mandelbrot cardioid/bulb rejection to avoid unnecessary iterations for points known to be inside the set.

## Project Layout

```txt
mandelbrot-cuda/
├── CMakeLists.txt
├── README.md
├── build/
├── output/
├── third_party/
│   └── stb_image_write.h
└── src/
    ├── main.cpp
    ├── mandelbrot.cu
    ├── mandelbrot.cuh
    ├── image_utils.cpp
    ├── image_utils.h
    ├── video_utils.cpp
    └── video_utils.h
```
