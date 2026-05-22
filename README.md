# Mandelbrot CUDA

A CUDA/CMake Mandelbrot renderer that generates high-resolution PNG stills and smooth MP4 zoom animations on the GPU. It uses smooth escape-time coloring, selectable perceptual colormaps, and an `ffmpeg` video pipeline for direct animation output.

## Demo

<video src="https://raw.githubusercontent.com/MOAI-cloud/Mandelbrot/main/assets/mandelbrot-zoom.mp4" controls autoplay muted loop playsinline width="100%"></video>

## Features

- GPU Mandelbrot rendering with CUDA
- PNG image output through `stb_image_write.h`
- MP4 zoom animation output through `ffmpeg`
- Smooth escape-time coloring for continuous gradients
- Colormap presets: `magma`, `inferno`, `viridis`, `cividis`, `turbo`, and `classic`
- Optional NVIDIA hardware encoding with `h264_nvenc` or `hevc_nvenc`
- Pinned host memory, a reusable device buffer, vectorized RGBA stores, and cardioid/bulb rejection for faster rendering

## Requirements

- NVIDIA GPU with a working CUDA driver
- CUDA Toolkit with `nvcc`
- CMake 3.18 or newer
- C++17-capable host compiler
- `ffmpeg` for MP4 video output

## Build

```sh
cmake -S . -B build
cmake --build build -j
```

## Quick Start

Render a default 1920x1080 PNG:

```sh
./build/mandelbrot_cuda --output output/mandelbrot.png
```

Render a detailed still image around a classic Seahorse Valley zoom target:

```sh
./build/mandelbrot_cuda \
    --width 2560 \
    --height 1440 \
    --iterations 2000 \
    --center-x -0.743643887037151 \
    --center-y 0.13182590420533 \
    --scale 0.002 \
    --colormap inferno \
    --output output/zoom.png
```

Render a README-friendly 720p zoom video:

```sh
./build/mandelbrot_cuda \
    --width 1280 \
    --height 720 \
    --iterations 2500 \
    --center-x -0.743643887037151 \
    --center-y 0.13182590420533 \
    --scale 0.01 \
    --end-scale 0.00002 \
    --frames 300 \
    --fps 30 \
    --colormap inferno \
    --no-image \
    --video output/mandelbrot-zoom.mp4
```

If your `ffmpeg` build supports NVIDIA hardware encoding, add `--video-encoder h264_nvenc` for faster MP4 encoding. `libx264` is the default because it is widely available and reliable on machines without NVENC.

## Options

| Option | Description |
| --- | --- |
| `--width <pixels>` | Image width, default `1920` |
| `--height <pixels>` | Image height, default `1080` |
| `--iterations <count>` | Maximum iterations per pixel, default `1000` |
| `--center-x <value>` | View center on the real axis, default `-0.75` |
| `--center-y <value>` | View center on the imaginary axis, default `0.0` |
| `--scale <value>` | Vertical view size, default `3.0` |
| `--output <path>` | Output PNG path, default `output/mandelbrot.png` |
| `--colormap <name>` | Colormap: `magma`, `inferno`, `viridis`, `cividis`, `turbo`, or `classic`; default `magma` |
| `--no-image` | Skip PNG output when rendering video |
| `--video <path>` | Output MP4 path and enable video rendering |
| `--frames <count>` | Video frame count, default `180` when video is enabled |
| `--fps <count>` | Video frames per second, default `30` |
| `--video-encoder <name>` | `ffmpeg` encoder: `libx264`, `h264_nvenc`, or `hevc_nvenc`; default `libx264` |
| `--end-scale <value>` | Final video scale, default `2%` of `--scale` |
| `--help` | Show usage |

## How The Zoom Works

For video output, the renderer interpolates from `--scale` to `--end-scale` over the requested frame count. The scale transition uses smoothstep easing, so the zoom starts and ends gently while moving faster through the middle of the animation.

## Project Layout

```txt
mandelbrot-cuda/
├── assets/
│   └── mandelbrot-zoom.mp4
├── CMakeLists.txt
├── README.md
├── output/
├── src/
│   ├── image_utils.cpp
│   ├── image_utils.h
│   ├── main.cpp
│   ├── mandelbrot.cu
│   ├── mandelbrot.cuh
│   ├── video_utils.cpp
│   └── video_utils.h
└── third_party/
    └── stb_image_write.h
```
