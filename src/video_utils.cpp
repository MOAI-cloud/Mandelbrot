#include "video_utils.h"

#include <cstdlib>

namespace {

std::string quote_for_shell(const std::string& value) {
    std::string quoted("'");

    for (const char character : value) {
        if (character == '\'') {
            quoted += "'\\''";
        } else {
            quoted += character;
        }
    }

    quoted += '\'';
    return quoted;
}

} // namespace

bool ffmpeg_available() {
    return std::system("command -v ffmpeg >/dev/null 2>&1") == 0;
}

const char* video_encoder_name(VideoEncoder encoder) {
    switch (encoder) {
    case VideoEncoder::H264Nvenc:
        return "h264_nvenc";
    case VideoEncoder::HevcNvenc:
        return "hevc_nvenc";
    case VideoEncoder::Libx264:
    default:
        return "libx264";
    }
}

VideoWriter::VideoWriter(const std::string& file_path, int width, int height, int fps, VideoEncoder encoder) {
    if (file_path.empty() || width <= 0 || height <= 0 || fps <= 0) {
        return;
    }

    const bool use_nvenc = encoder == VideoEncoder::H264Nvenc || encoder == VideoEncoder::HevcNvenc;
    const std::string encoder_options = use_nvenc ? " -preset fast -cq 18" : " -preset veryfast -crf 18";

    const std::string command =
        "ffmpeg -y -loglevel error -f rawvideo -pix_fmt rgba -s:v " + std::to_string(width) + "x" +
        std::to_string(height) + " -r " + std::to_string(fps) +
        " -i - -an -c:v " + video_encoder_name(encoder) + encoder_options + " -pix_fmt yuv420p" +
        " -movflags +faststart " +
        quote_for_shell(file_path);

    pipe_ = ::popen(command.c_str(), "w");
}

VideoWriter::~VideoWriter() {
    close();
}

bool VideoWriter::is_open() const {
    return pipe_ != nullptr;
}

bool VideoWriter::write_frame(const std::uint8_t* rgba_pixels, std::size_t byte_count) {
    if (pipe_ == nullptr || rgba_pixels == nullptr || byte_count == 0) {
        return false;
    }

    return std::fwrite(rgba_pixels, 1, byte_count, pipe_) == byte_count;
}

bool VideoWriter::close() {
    if (pipe_ == nullptr) {
        return true;
    }

    std::FILE* pipe = pipe_;
    pipe_ = nullptr;
    return ::pclose(pipe) == 0;
}