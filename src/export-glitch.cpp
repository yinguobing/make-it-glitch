// This is a sample code of generating and exporting glitchy video frames with
// FFMPEG.
// For more: https://github.com/yinguobing/make-it-glitch

#include "video_decoder.hpp"

int main(int argc, char** argv)
{
    // Safety check, always!
    if (argc != 3) {
        std::cout << "Usage:\n"
                  << argv[0] << " <your-video-file> <export-dir>" << std::endl;
        exit(1);
    }

    // Create directories for exporting storage.
    std::filesystem::path export_dir { argv[2] };
    std::filesystem::path original_dir { "original" };
    std::filesystem::path glitch_dir { "glitchy" };
    std::filesystem::create_directories(export_dir / original_dir);
    std::filesystem::create_directories(export_dir / glitch_dir);

    // Init the decoder
    char* filename = argv[1];
    VideoDecoder decoder { std::string(filename), AV_HWDEVICE_TYPE_CUDA };

    std::cout << "Supported accelerator: ";
    for (auto&& acc : decoder.list_hw_accelerators())
        std::cout << acc << " " << std::endl;
    std::cout << "Valid: " << decoder.is_valid() << std::endl;
    std::cout << "Accelerated: " << decoder.is_accelerated() << std::endl;

    uint8_t* buffer = decoder.get_buffer();
    auto [width, height] = decoder.get_frame_dims();
    std::cout << "Width: " << width << " height: " << height << std::endl;

    cv::Mat bgr(height, width, CV_8UC3, buffer, decoder.get_frame_steps());
    int ret = 0;
    while (ret == 0 or ret == AVERROR(EAGAIN)) {
        ret = decoder.read(true);
        cv::imshow("preview", bgr);
        if (cv::waitKey(1) == 27)
            break;
    }

    return 0;
}
