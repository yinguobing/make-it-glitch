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

class VideoDecoder {
private:
    // Contexts
    AVFormatContext* ctx_format = nullptr;
    AVCodecContext* ctx_decode = nullptr;
    SwsContext* ctx_sws = nullptr;

    // Decoder
    AVCodec* decoder = nullptr;
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
    bool is_valid();
    bool is_accelerated();
    std::pair<int, int> get_frame_dims();
    int get_frame_steps();
    void random_touch();

    // List available hardware accelerators.
    std::vector<std::string> list_hw_accelerators();

    // Get the frame buffer.
    uint8_t* get_buffer();

    // Use this to push the decoded frame data to buf.
    int read(bool touch = false);
};
#endif // VIDEO_DECODER_HPP
