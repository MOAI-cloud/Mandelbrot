#include "image_utils.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

bool save_png(const std::string& file_path, const std::uint8_t* rgba_pixels, int width, int height) {
    if (rgba_pixels == nullptr || width <= 0 || height <= 0) {
        return false;
    }

    constexpr int channels = 4;
    // stb writes row-major RGBA pixels; the stride is the byte distance between rows.
    const int stride_in_bytes = width * channels;

    return stbi_write_png(file_path.c_str(), width, height, channels, rgba_pixels, stride_in_bytes) != 0;
}
