#pragma once

#include <cuda_runtime.h>

#include <cstdint>

// Color ramps supported by the CUDA renderer.
enum class MandelbrotColormap : int {
    Classic,
    Magma,
    Inferno,
    Viridis,
    Cividis,
    Turbo,
};

// Launches Mandelbrot rendering into device_image and returns the CUDA launch status.
cudaError_t launch_mandelbrot(std::uint8_t* device_image,
                              int width,
                              int height,
                              int max_iterations,
                              double center_x,
                              double center_y,
                              double scale,
                              cudaStream_t stream = nullptr,
                              MandelbrotColormap colormap = MandelbrotColormap::Magma);
