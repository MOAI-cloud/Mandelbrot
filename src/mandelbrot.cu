#include "mandelbrot.cuh"

namespace {

struct ColorRgb {
    double red;
    double green;
    double blue;
};

// Small palette tables live in constant GPU memory because every thread reads them.
__device__ __constant__ ColorRgb magma_palette[] = {
    {0.0000, 0.0000, 0.0157}, {0.0941, 0.0588, 0.2392}, {0.2667, 0.0588, 0.4627},
    {0.4471, 0.1216, 0.5059}, {0.6196, 0.1843, 0.4980}, {0.8039, 0.2510, 0.4431},
    {0.9451, 0.3765, 0.3647}, {0.9922, 0.5882, 0.4078}, {0.9961, 0.7922, 0.5529},
    {0.9882, 0.9922, 0.7490},
};

__device__ __constant__ ColorRgb inferno_palette[] = {
    {0.0000, 0.0000, 0.0157}, {0.1059, 0.0471, 0.2549}, {0.2902, 0.0471, 0.4196},
    {0.4706, 0.1098, 0.4275}, {0.6471, 0.1725, 0.3765}, {0.8118, 0.2667, 0.2745},
    {0.9294, 0.4118, 0.1451}, {0.9843, 0.6078, 0.0235}, {0.9686, 0.8196, 0.2392},
    {0.9882, 1.0000, 0.6431},
};

__device__ __constant__ ColorRgb viridis_palette[] = {
    {0.2667, 0.0039, 0.3294}, {0.2824, 0.1569, 0.4706}, {0.2431, 0.2863, 0.5373},
    {0.1922, 0.4078, 0.5569}, {0.1490, 0.5098, 0.5569}, {0.1216, 0.6196, 0.5373},
    {0.2078, 0.7176, 0.4745}, {0.4314, 0.8078, 0.3451}, {0.7098, 0.8706, 0.1686},
    {0.9922, 0.9059, 0.1451},
};

__device__ __constant__ ColorRgb cividis_palette[] = {
    {0.0000, 0.1255, 0.2980}, {0.0706, 0.2078, 0.4392}, {0.2314, 0.2863, 0.4235},
    {0.3412, 0.3647, 0.4275}, {0.4392, 0.4431, 0.4510}, {0.5412, 0.5255, 0.4706},
    {0.6471, 0.6118, 0.4549}, {0.7647, 0.7020, 0.4118}, {0.8824, 0.8000, 0.3333},
    {1.0000, 0.9176, 0.2745},
};

__device__ __constant__ ColorRgb turbo_palette[] = {
    {0.1882, 0.0706, 0.2314}, {0.2667, 0.3294, 0.7647}, {0.2431, 0.6078, 0.9961},
    {0.0941, 0.8471, 0.8314}, {0.2745, 0.9725, 0.5176}, {0.6353, 0.9882, 0.2353},
    {0.8824, 0.8667, 0.2157}, {0.9882, 0.6510, 0.2118}, {0.9373, 0.3529, 0.0667},
    {0.7176, 0.1333, 0.0118},
};

constexpr int palette_size = 10;

// Skip iteration for regions that are analytically known to be inside the set.
__device__ __forceinline__ bool is_inside_cardioid_or_bulb(double point_x, double point_y) {
    const double point_y_squared = point_y * point_y;
    const double shifted_x = point_x - 0.25;
    const double q = shifted_x * shifted_x + point_y_squared;

    if (q * (q + shifted_x) <= 0.25 * point_y_squared) {
        return true;
    }

    const double bulb_x = point_x + 1.0;
    return bulb_x * bulb_x + point_y_squared <= 0.0625;
}

__device__ __forceinline__ double clamp01(double value) {
    if (value < 0.0) {
        return 0.0;
    }

    if (value > 1.0) {
        return 1.0;
    }

    return value;
}

__device__ __forceinline__ unsigned char color_to_byte(double value) {
    return static_cast<unsigned char>(clamp01(value) * 255.0 + 0.5);
}

// Interpolate palette stops so neighboring iteration counts blend smoothly.
__device__ __forceinline__ ColorRgb sample_palette(const ColorRgb* palette, double value) {
    const double scaled_value = clamp01(value) * static_cast<double>(palette_size - 1);
    const int lower_index = static_cast<int>(scaled_value);

    if (lower_index >= palette_size - 1) {
        return palette[palette_size - 1];
    }

    const double blend = scaled_value - static_cast<double>(lower_index);
    const ColorRgb lower = palette[lower_index];
    const ColorRgb upper = palette[lower_index + 1];

    return {lower.red + (upper.red - lower.red) * blend,
            lower.green + (upper.green - lower.green) * blend,
            lower.blue + (upper.blue - lower.blue) * blend};
}

__device__ __forceinline__ ColorRgb sample_colormap(MandelbrotColormap colormap, double value) {
    switch (colormap) {
    case MandelbrotColormap::Classic: {
        const double inverse = 1.0 - value;
        return {9.0 * inverse * value * value * value,
                15.0 * inverse * inverse * value * value,
                8.5 * inverse * inverse * inverse * value};
    }
    case MandelbrotColormap::Inferno:
        return sample_palette(inferno_palette, value);
    case MandelbrotColormap::Viridis:
        return sample_palette(viridis_palette, value);
    case MandelbrotColormap::Cividis:
        return sample_palette(cividis_palette, value);
    case MandelbrotColormap::Turbo:
        return sample_palette(turbo_palette, value);
    case MandelbrotColormap::Magma:
    default:
        return sample_palette(magma_palette, value);
    }
}

// Smooth escape-time coloring removes hard bands between integer iterations.
__device__ __forceinline__ double smooth_color_value(int iteration,
                                                     int max_iterations,
                                                     double real_squared,
                                                     double imaginary_squared) {
    const double magnitude_squared = real_squared + imaginary_squared;
    const double log_magnitude = 0.5 * log(magnitude_squared);
    const double smooth_iteration = static_cast<double>(iteration) + 1.0 - log(log_magnitude) / log(2.0);
    const double normalized_iteration = clamp01(smooth_iteration / static_cast<double>(max_iterations));

    return pow(normalized_iteration, 0.35);
}

// Each CUDA thread shades one output pixel in the RGBA image.
__global__ __launch_bounds__(256) void mandelbrot_kernel(uchar4* __restrict__ image,
                                                         int width,
                                                         int height,
                                                         int max_iterations,
                                                         double min_x,
                                                         double min_y,
                                                         double x_step,
                                                         double y_step,
                                                         MandelbrotColormap colormap) {
    const int pixel_x = blockIdx.x * blockDim.x + threadIdx.x;
    const int pixel_y = blockIdx.y * blockDim.y + threadIdx.y;

    if (pixel_x >= width || pixel_y >= height) {
        return;
    }

    const double point_x = min_x + static_cast<double>(pixel_x) * x_step;
    const double point_y = min_y + static_cast<double>(pixel_y) * y_step;
    const int output_index = pixel_y * width + pixel_x;

    if (is_inside_cardioid_or_bulb(point_x, point_y)) {
        image[output_index] = make_uchar4(0, 0, 0, 255);
        return;
    }

    double real = 0.0;
    double imaginary = 0.0;
    double real_squared = 0.0;
    double imaginary_squared = 0.0;
    int iteration = 0;

    while (real_squared + imaginary_squared <= 4.0 && iteration < max_iterations) {
        const double next_real = real_squared - imaginary_squared + point_x;
        imaginary = 2.0 * real * imaginary + point_y;
        real = next_real;
        real_squared = real * real;
        imaginary_squared = imaginary * imaginary;
        ++iteration;
    }

    if (iteration == max_iterations) {
        image[output_index] = make_uchar4(0, 0, 0, 255);
        return;
    }

    const double color_value = smooth_color_value(iteration, max_iterations, real_squared, imaginary_squared);
    const ColorRgb color = sample_colormap(colormap, color_value);

    image[output_index] = make_uchar4(color_to_byte(color.red),
                                      color_to_byte(color.green),
                                      color_to_byte(color.blue),
                                      255);
}

}

cudaError_t launch_mandelbrot(std::uint8_t* device_image,
                              int width,
                              int height,
                              int max_iterations,
                              double center_x,
                              double center_y,
                              double scale,
                              cudaStream_t stream,
                              MandelbrotColormap colormap) {
    // Convert the requested center and vertical scale into complex-plane bounds.
    const double aspect_ratio = static_cast<double>(width) / static_cast<double>(height);
    const double view_width = scale * aspect_ratio;
    const double min_x = center_x - view_width * 0.5;
    const double min_y = center_y - scale * 0.5;
    const double x_step = view_width / static_cast<double>(width);
    const double y_step = scale / static_cast<double>(height);

    // 32 x 8 keeps 256 threads per block while covering neighboring x pixels together.
    const dim3 threads_per_block(32, 8);
    const dim3 blocks_per_grid((width + threads_per_block.x - 1) / threads_per_block.x,
                               (height + threads_per_block.y - 1) / threads_per_block.y);

    auto* image = reinterpret_cast<uchar4*>(device_image);
    mandelbrot_kernel<<<blocks_per_grid, threads_per_block, 0, stream>>>(image, width, height, max_iterations,
                                                                        min_x, min_y, x_step, y_step, colormap);

    return cudaGetLastError();
}
