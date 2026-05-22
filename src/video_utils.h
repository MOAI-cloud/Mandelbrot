#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

// ffmpeg encoders exposed by the command-line interface.
enum class VideoEncoder {
    Libx264,
    H264Nvenc,
    HevcNvenc,
};

bool ffmpeg_available();
const char* video_encoder_name(VideoEncoder encoder);

// Streams raw RGBA frames into an ffmpeg process.
class VideoWriter {
  public:
    VideoWriter(const std::string& file_path, int width, int height, int fps, VideoEncoder encoder);
    ~VideoWriter();

    VideoWriter(const VideoWriter&) = delete;
    VideoWriter& operator=(const VideoWriter&) = delete;

    bool is_open() const;
    bool write_frame(const std::uint8_t* rgba_pixels, std::size_t byte_count);
    bool close();

  private:
    std::FILE* pipe_ = nullptr;
};