// This is a sample code of generating and exporting glitchy video frames with
// FFMPEG.
// For more: https://github.com/yinguobing/make-it-glitch

#include "video_decoder.hpp"

int main(int argc, char** argv)
{
    // Safety check, always!
    if (argc != 3 and argc != 4) {
        std::cout << "Usage:\n    "
                  << argv[0] << " <your-video-file> <export-dir> [no-touching]\n"
                  << "More than 4 args will trigger the exporting without any glitch(the original frame)."
                  << std::endl;
        exit(1);
    }

    // Create directories for exporting images.
    std::filesystem::path video_file { argv[1] };
    std::filesystem::path export_dir { argv[2] };
    std::filesystem::create_directories(export_dir);
    std::cout << "Glitchy images will be saved in " << export_dir.string() << std::endl;

    // Init the decoder
    char* filename = argv[1];
    VideoDecoder decoder { std::string(filename), AV_HWDEVICE_TYPE_CUDA };

    // Check if the decoder is valid
    std::cout << "Supported accelerator: ";
    for (auto&& acc : decoder.list_hw_accelerators())
        std::cout << acc << " " << std::endl;
    std::cout << "Valid: " << decoder.is_valid() << std::endl;
    std::cout << "Accelerated: " << decoder.is_accelerated() << std::endl;

    // Prepare memory space for frames
    uint8_t* buffer = decoder.get_buffer();
    auto [width, height] = decoder.get_frame_dims();
    std::cout << "Width: " << width << " height: " << height << std::endl;
    cv::Mat bgr(height, width, CV_8UC3, buffer, decoder.get_frame_steps());

    // Loop the video stream for frames. Press `ESC` to stop.
    int ret = 0, frame_count = 0, frame_skip = 25;
    bool will_be_touched = argc == 3;
    while (ret == 0 or ret == AVERROR(EAGAIN)) {
        frame_count++;
        ret = decoder.read(will_be_touched);
        if (frame_count % frame_skip == 0) {
            std::string filename = video_file.stem().string().append("-").append(std::to_string(frame_count)).append(".jpg");
            auto img_path = export_dir / std::filesystem::path { filename };
            cv::imwrite(img_path.string(), bgr);
        }
        cv::imshow("preview", bgr);
        if (cv::waitKey(1) == 27)
            break;
    }

    return 0;
}
