#pragma once

#include <cstdint>
#include <string>

// Saves tightly packed RGBA pixels as a PNG file.
bool save_png(const std::string& file_path, const std::uint8_t* rgba_pixels, int width, int height);
