#include "image_utils.h"
#include "mandelbrot.cuh"
#include "video_utils.h"
#include <cuda_runtime.h>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

struct Options {
    int width = 1920;
    int height = 1080;
    int iterations = 1000;
    double center_x = -0.75;
    double center_y = 0.0;
    double scale = 3.0;
    std::string output_path = "output/mandelbrot.png";
    bool save_image = true;
    std::string video_path;
    int video_frames = 0;
    int video_fps = 30;
    double video_end_scale = 0.0;
    MandelbrotColormap colormap = MandelbrotColormap::Magma;
    VideoEncoder video_encoder = VideoEncoder::Libx264;

    bool has_video() const {
        return !video_path.empty();
    }
};

void print_usage(const char* program_name) {
    std::cout
        << "Usage: " << program_name << " [options]\n\n"
        << "Options:\n"
        << "  --width <pixels>        Image width, default 1920\n"
        << "  --height <pixels>       Image height, default 1080\n"
        << "  --iterations <count>    Maximum iterations per pixel, default 1000\n"
        << "  --center-x <value>      View center on the real axis, default -0.75\n"
        << "  --center-y <value>      View center on the imaginary axis, default 0.0\n"
        << "  --scale <value>         Vertical view size, default 3.0\n"
        << "  --output <path>         Output PNG path, default output/mandelbrot.png\n"
        << "  --colormap <name>       Colormap: magma, inferno, viridis, cividis, turbo, classic; default magma\n"
        << "  --no-image             Skip PNG output when rendering video\n"
        << "  --video <path>         Output MP4 path and enable video rendering\n"
        << "  --frames <count>       Video frame count, default 180 when video is enabled\n"
        << "  --fps <count>          Video frames per second, default 30\n"
        << "  --video-encoder <name> ffmpeg encoder: libx264, h264_nvenc, hevc_nvenc; default libx264\n"
        << "  --end-scale <value>    Final video scale, default 2% of --scale\n"
        << "  --help                  Show this help message\n";
}

int parse_positive_int(const std::string& option_name, const char* value) {
    const std::string text(value);
    std::size_t consumed_chars = 0;
    const int parsed_value = std::stoi(text, &consumed_chars);

    if (consumed_chars != text.size() || parsed_value <= 0) {
        throw std::runtime_error(option_name + " requires a positive integer");
    }

    return parsed_value;
}

double parse_double(const std::string& option_name, const char* value) {
    const std::string text(value);
    std::size_t consumed_chars = 0;
    const double parsed_value = std::stod(text, &consumed_chars);

    if (consumed_chars != text.size()) {
        throw std::runtime_error(option_name + " requires a numeric value");
    }

    return parsed_value;
}

MandelbrotColormap parse_colormap(const char* value) {
    const std::string text(value);

    if (text == "magma") {
        return MandelbrotColormap::Magma;
    }
    if (text == "inferno") {
        return MandelbrotColormap::Inferno;
    }
    if (text == "viridis") {
        return MandelbrotColormap::Viridis;
    }
    if (text == "cividis") {
        return MandelbrotColormap::Cividis;
    }
    if (text == "turbo") {
        return MandelbrotColormap::Turbo;
    }
    if (text == "classic") {
        return MandelbrotColormap::Classic;
    }

    throw std::runtime_error("--colormap must be one of: magma, inferno, viridis, cividis, turbo, classic");
}

VideoEncoder parse_video_encoder(const char* value) {
    const std::string text(value);

    if (text == "libx264") {
        return VideoEncoder::Libx264;
    }
    if (text == "h264_nvenc") {
        return VideoEncoder::H264Nvenc;
    }
    if (text == "hevc_nvenc") {
        return VideoEncoder::HevcNvenc;
    }

    throw std::runtime_error("--video-encoder must be one of: libx264, h264_nvenc, hevc_nvenc");
}

Options parse_options(int argc, char** argv) {
    Options options;
    bool video_requested = false;

    for (int argument_index = 1; argument_index < argc; ++argument_index) {
        const std::string argument(argv[argument_index]);

        const auto require_value = [&]() -> const char* {
            if (argument_index + 1 >= argc) {
                throw std::runtime_error(argument + " requires a value");
            }
            return argv[++argument_index];
        };

        if (argument == "--help" || argument == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (argument == "--width") {
            options.width = parse_positive_int(argument, require_value());
        } else if (argument == "--height") {
            options.height = parse_positive_int(argument, require_value());
        } else if (argument == "--iterations") {
            options.iterations = parse_positive_int(argument, require_value());
        } else if (argument == "--center-x") {
            options.center_x = parse_double(argument, require_value());
        } else if (argument == "--center-y") {
            options.center_y = parse_double(argument, require_value());
        } else if (argument == "--scale") {
            options.scale = parse_double(argument, require_value());
            if (options.scale <= 0.0) {
                throw std::runtime_error("--scale requires a positive numeric value");
            }
        } else if (argument == "--output") {
            options.output_path = require_value();
        } else if (argument == "--colormap") {
            options.colormap = parse_colormap(require_value());
        } else if (argument == "--no-image") {
            options.save_image = false;
        } else if (argument == "--video") {
            options.video_path = require_value();
            video_requested = true;
        } else if (argument == "--frames") {
            options.video_frames = parse_positive_int(argument, require_value());
            video_requested = true;
        } else if (argument == "--fps") {
            options.video_fps = parse_positive_int(argument, require_value());
        } else if (argument == "--video-encoder") {
            options.video_encoder = parse_video_encoder(require_value());
        } else if (argument == "--end-scale") {
            options.video_end_scale = parse_double(argument, require_value());
            if (options.video_end_scale <= 0.0) {
                throw std::runtime_error("--end-scale requires a positive numeric value");
            }
            video_requested = true;
        } else {
            throw std::runtime_error("Unknown option: " + argument);
        }
    }

    if (video_requested && options.video_path.empty()) {
        options.video_path = "output/mandelbrot.mp4";
    }

    if (options.has_video() && options.video_frames == 0) {
        options.video_frames = 180;
    }

    if (options.has_video() && options.video_end_scale == 0.0) {
        options.video_end_scale = options.scale * 0.02;
    }

    if (!options.save_image && !options.has_video()) {
        throw std::runtime_error("--no-image requires --video, --frames, or --end-scale");
    }

    return options;
}

void check_cuda(cudaError_t status, const char* expression) {
    if (status != cudaSuccess) {
        throw std::runtime_error(std::string(expression) + " failed: " + cudaGetErrorString(status));
    }
}

#define CUDA_CHECK(call) check_cuda((call), #call)

class CudaStream {
  public:
    CudaStream() {
        CUDA_CHECK(cudaStreamCreateWithFlags(&stream_, cudaStreamNonBlocking));
    }

    ~CudaStream() {
        if (stream_ != nullptr) {
            cudaStreamDestroy(stream_);
        }
    }

    CudaStream(const CudaStream&) = delete;
    CudaStream& operator=(const CudaStream&) = delete;

    cudaStream_t get() const {
        return stream_;
    }

  private:
    cudaStream_t stream_ = nullptr;
};

class FrameBuffers {
  public:
    explicit FrameBuffers(std::size_t byte_count) : byte_count_(byte_count) {
        CUDA_CHECK(cudaMallocHost(reinterpret_cast<void**>(&host_image_), byte_count_));

        try {
            CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&device_image_), byte_count_));
        } catch (...) {
            cudaFreeHost(host_image_);
            host_image_ = nullptr;
            throw;
        }
    }

    ~FrameBuffers() {
        if (device_image_ != nullptr) {
            cudaFree(device_image_);
        }
        if (host_image_ != nullptr) {
            cudaFreeHost(host_image_);
        }
    }

    FrameBuffers(const FrameBuffers&) = delete;
    FrameBuffers& operator=(const FrameBuffers&) = delete;

    std::uint8_t* host_data() const {
        return host_image_;
    }

    std::uint8_t* device_data() const {
        return device_image_;
    }

    std::size_t byte_count() const {
        return byte_count_;
    }

  private:
    std::uint8_t* host_image_ = nullptr;
    std::uint8_t* device_image_ = nullptr;
    std::size_t byte_count_ = 0;
};

void create_parent_directory(const std::string& file_path) {
    const std::filesystem::path output_path(file_path);
    if (output_path.has_parent_path()) {
        std::filesystem::create_directories(output_path.parent_path());
    }
}

void render_frame(const Options& options, double scale, FrameBuffers& buffers, cudaStream_t stream) {
    CUDA_CHECK(launch_mandelbrot(buffers.device_data(), options.width, options.height, options.iterations,
                                 options.center_x, options.center_y, scale, stream, options.colormap));
    CUDA_CHECK(cudaMemcpyAsync(buffers.host_data(), buffers.device_data(), buffers.byte_count(), cudaMemcpyDeviceToHost,
                               stream));
    CUDA_CHECK(cudaStreamSynchronize(stream));
}

double video_scale_for_frame(const Options& options, int frame_index) {
    if (options.video_frames <= 1) {
        return options.video_end_scale;
    }

    const double progress = static_cast<double>(frame_index) / static_cast<double>(options.video_frames - 1);
    const double eased_progress = progress * progress * (3.0 - 2.0 * progress);

    return options.scale * std::pow(options.video_end_scale / options.scale, eased_progress);
}

void save_still_image(const Options& options, FrameBuffers& buffers, cudaStream_t stream) {
    render_frame(options, options.scale, buffers, stream);
    create_parent_directory(options.output_path);

    if (!save_png(options.output_path, buffers.host_data(), options.width, options.height)) {
        throw std::runtime_error("Failed to save image: " + options.output_path);
    }

    std::cout << "Saved \"" << options.output_path << "\"\n";
}

void save_video(const Options& options, FrameBuffers& buffers, cudaStream_t stream) {
    if (!ffmpeg_available()) {
        throw std::runtime_error("ffmpeg is required for --video output. Install ffmpeg and run again.");
    }

    create_parent_directory(options.video_path);
    VideoWriter video_writer(options.video_path, options.width, options.height, options.video_fps, options.video_encoder);
    if (!video_writer.is_open()) {
        throw std::runtime_error("Failed to start ffmpeg for video output: " + options.video_path);
    }

    std::cout << "Encoding video with " << video_encoder_name(options.video_encoder) << "\n";

    const int progress_interval = options.video_frames < 10 ? 1 : options.video_frames / 10;
    for (int frame_index = 0; frame_index < options.video_frames; ++frame_index) {
        render_frame(options, video_scale_for_frame(options, frame_index), buffers, stream);

        if (!video_writer.write_frame(buffers.host_data(), buffers.byte_count())) {
            throw std::runtime_error("Failed to write video frame to ffmpeg");
        }

        if ((frame_index + 1) % progress_interval == 0 || frame_index + 1 == options.video_frames) {
            std::cout << "\rRendered video frame " << (frame_index + 1) << '/' << options.video_frames << std::flush;
        }
    }

    if (!video_writer.close()) {
        throw std::runtime_error("ffmpeg failed while finalizing video: " + options.video_path);
    }

    std::cout << "\nSaved \"" << options.video_path << "\"\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);
        const std::size_t pixel_count = static_cast<std::size_t>(options.width) * static_cast<std::size_t>(options.height);
        FrameBuffers buffers(pixel_count * 4 * sizeof(std::uint8_t));
        CudaStream render_stream;

        if (options.save_image) {
            save_still_image(options, buffers, render_stream.get());
        }

        if (options.has_video()) {
            save_video(options, buffers, render_stream.get());
        }

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        std::cerr << "Run with --help to see available options.\n";
        return 1;
    }
}
