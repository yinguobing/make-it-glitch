#if !defined(VIDEO_DECODER_HPP)
#define VIDEO_DECODER_HPP

#include <filesystem>
#include <random>
#include <string>

#include "opencv2/highgui.hpp"
#include "opencv2/opencv.hpp"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/hwcontext.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
}

/// @brief A simple wrapper for video decoding.
class VideoDecoder {
private:
    // Contexts
    AVFormatContext* ctx_format = nullptr;
    AVCodecContext* ctx_decode = nullptr;
    SwsContext* ctx_sws = nullptr;

    // Decoder
    const AVCodec* decoder = nullptr;
    AVStream* stream = nullptr;
    int stream_index;

    // Packet
    AVPacket* packet = nullptr;

    // Frames
    AVFrame* frame = nullptr; // in system memory
    AVFrame* frame_hw = nullptr; // in hardware memory
    AVFrame* frame_bgr = nullptr; // in system memory, BGR format

    // Hardware accelerations
    std::vector<AVHWDeviceType> hw_accelerators;
    AVHWDeviceType enabled_hw_accelerator = AV_HWDEVICE_TYPE_NONE;
    static AVPixelFormat hw_pix_fmt;
    AVBufferRef* hw_device_ctx = nullptr;
    void query_supported_hw_devices(std::vector<AVHWDeviceType>& hw_accelerators);
    static AVPixelFormat get_hw_format(AVCodecContext* ctx, const AVPixelFormat* pix_fmts);

    // Format convert
    AVPixelFormat output_fmt = AV_PIX_FMT_BGR24;
    int to_bgr();

    // Some flags
    bool initialized = false;
    bool hw_acc_enabled = false;

public:
    VideoDecoder(const std::string url, AVHWDeviceType hw_acc = AV_HWDEVICE_TYPE_NONE);
    ~VideoDecoder();

    /// @brief check if the decoder was successfully initialized.
    /// @return true if the decoder is valid, else false.
    bool is_valid();

    /// @brief Check if the decoder is hardware accelerated.
    /// @return true if accelerated, else false.
    bool is_accelerated();

    /// @brief Get the frame size.
    /// @return a std::pair of <width, height>
    std::pair<int, int> get_frame_dims();

    /// @brief Get the frame's step size. This is used for constructing OpenCV Mat.
    /// @return the step.
    int get_frame_steps();

    /// @brief Touch the decoding packet data, randomly.
    void random_touch();

    /// @brief List available hardware accelerators.
    /// @return a vector of accelerator names.
    std::vector<std::string> list_hw_accelerators();

    /// @brief  Get the BGR frame buffer.
    /// @return the pointer of pixel data.
    uint8_t* get_buffer();

    /// @brief Read a frame to buf.
    /// @param touch if true, the packet data will be touched randomly.
    /// @return 0 if success, -11 if try again is required, other negative for errors.
    int read(bool touch = false);
};
#endif // VIDEO_DECODER_HPP
