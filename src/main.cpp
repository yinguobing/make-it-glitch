// This is a C++ demo code of generating glitched video file with FFMPEG.
// For more: https://github.com/yinguobing/make-it-glitch

#include <random>
#include <string>

#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

static void query_supported_hw_devices(std::vector<AVHWDeviceType>& types)
{
    AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
    while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
        types.push_back(type);
}

int main(int argc, char** argv)
{
    // Safety check, always!
    if (argc != 2) {
        std::cout << "Usage:\n"
                  << argv[0] << " <your-video-file>" << std::endl;
        exit(1);
    }

    // What kind of media format we are going to face?
    char* filename = argv[1];
    int ret;
    AVFormatContext* format_context = nullptr;
    if ((ret = avformat_open_input(&format_context, filename, nullptr, nullptr)) < 0) {
        std::cerr << "Cannot open input file:" << filename << std::endl;
        return ret;
    }
    if ((ret = avformat_find_stream_info(format_context, nullptr)) < 0) {
        std::cerr << "Cannot find stream information." << std::endl;
        return ret;
    }

    // Is there any valid video stream to be processed?
    AVStream* stream;
    AVCodec* decoder;
    int stream_index;
    if ((stream_index = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0)) < 0) {
        std::cerr << "Cannot find valid stream: " << av_get_media_type_string(AVMEDIA_TYPE_VIDEO) << std::endl;
        return stream_index;
    } else {
        stream = format_context->streams[stream_index];
        std::cout << "Found video stream with index: " << stream_index << std::endl;
    }

    // Is there a valid decoder for the target media?
    if (!decoder) {
        std::cerr << "Cannot find decoder for stream: " << av_get_media_type_string(AVMEDIA_TYPE_VIDEO) << std::endl;
        return AVERROR_DECODER_NOT_FOUND;
    } else {
        std::cout << "Found video decoder: " << decoder->long_name << std::endl;
    }

    // Is any hardware acceleration available? The first device will be picked
    // if more than one were found.
    std::vector<AVHWDeviceType> hw_types;
    query_supported_hw_devices(hw_types);
    AVHWDeviceType hw_type = hw_types.empty() ? AV_HWDEVICE_TYPE_NONE : hw_types[0];
    bool hw_acc = false;
    if (hw_types.empty()) {
        std::cout << "No hardware acceleration available." << std::endl;
    } else {
        hw_acc = true;
        std::cout << "Found hardware accelerations:";
        for (auto&& t : hw_types)
            std::cout << " " << av_hwdevice_get_type_name(t);
        std::cout << std::endl;
        std::cout << "Acceleration to be used: " << av_hwdevice_get_type_name(hw_type) << std::endl;
    }

    // Is this decoder supported by the hardware?
    static AVPixelFormat hw_pix_fmt;
    if (hw_acc) {
        for (int i = 0;; i++) {
            const AVCodecHWConfig* config = avcodec_get_hw_config(decoder, i);
            if (!config) {
                std::cerr << "Decoder not supported by device: " << av_hwdevice_get_type_name(hw_type) << std::endl;
                return -1;
            }
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == hw_type) {
                hw_pix_fmt = config->pix_fmt;
                std::cout << "Acceleration pixel format: " << av_get_pix_fmt_name(hw_pix_fmt) << std::endl;
                break;
            }
        }
    }

    // Now it's time to init the decoder.
    AVCodecContext* decode_context = avcodec_alloc_context3(decoder);
    if (!decode_context) {
        std::cerr << "Cannot allocate decoder context." << std::endl;
        return AVERROR(ENOMEM);
    }
    if ((ret = avcodec_parameters_to_context(decode_context, stream->codecpar)) < 0) {
        std::cerr << "Cannot copy decoder parameters to input decoder context." << std::endl;
        return ret;
    }
    static AVBufferRef* hw_device_ctx = nullptr;
    if (hw_acc) {
        if ((ret = av_hwdevice_ctx_create(&hw_device_ctx, hw_type, nullptr, nullptr, 0)) < 0) {
            std::cerr << "Cannot create context for specified HW device." << std::endl;
            return ret;
        }
        decode_context->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    }
    if ((ret = avcodec_open2(decode_context, decoder, nullptr)) < 0) {
        std::cerr << "Cannot open decoder for stream: " << stream_index << std::endl;
        return ret;
    }

    // Create SWS Context for converting from decode pixel format (like YUV420) to BGR
    SwsContext *sws_ctx = nullptr, *cached_ctx = nullptr;
    sws_ctx = sws_getContext(decode_context->width,
        decode_context->height,
        decode_context->pix_fmt,
        decode_context->width,
        decode_context->height,
        AV_PIX_FMT_BGR24,
        SWS_BICUBIC,
        nullptr,
        nullptr,
        nullptr);
    if (sws_ctx == nullptr) {
        std::cerr << "Cannot init SWS context." << std::endl;
        return -1;
    }

    // Allocate frames: raw frame in system memory
    AVFrame* frame_yuv = nullptr;

    // Allocate frames: raw frame in HW memory
    AVFrame* frame_raw = nullptr;

    // Allocate frames: converted color frame
    AVFrame* frame_bgr = nullptr;

    if (!(frame_raw = av_frame_alloc()) or !(frame_bgr = av_frame_alloc()) or !(frame_yuv = av_frame_alloc())) {
        std::cerr << "Cannot allocate frames." << std::endl;
        return AVERROR(ENOMEM);
    }

    // Setup the converting frame
    frame_bgr->format = AV_PIX_FMT_BGR24;
    frame_bgr->width = decode_context->width;
    frame_bgr->height = decode_context->height;
    if (av_frame_get_buffer(frame_bgr, 0) < 0) {
        std::cerr << "Cannot allocate SWS frame buffer." << std::endl;
        return -1;
    }

    // Standard mersenne_twister_engine seeded with rd()
    std::random_device rd;
    std::mt19937 gen(rd());

    // Init the packet
    AVPacket* packet = nullptr;
    if (!(packet = av_packet_alloc())) {
        std::cerr << "Cannot allocate packet." << std::endl;
        return -1;
    }

    // Finally, loop the frames.
    while (av_read_frame(format_context, packet) >= 0) {
        // Is this packet valid?
        if (packet->stream_index == stream_index) {

            // Touch the data, to make it glitch!
            std::uniform_int_distribution<> location(0, packet->size - 1);
            std::uniform_int_distribution<> val(0, 8);
            int a = location(gen);
            int b = location(gen);
            int start = std::min(a, b);
            int end = std::max(a, b);
            end = start == end ? ++end : end;
            for (size_t i = start; i < end; i++) {
                packet->data[i] = val(gen);
            }

            // Send it to decoding
            ret = avcodec_send_packet(decode_context, packet);
            if (ret < 0) {
                std::cerr << "Error submitting a packet for decoding: " << ret << std::endl;
            } else {
                // Frame got?
                ret = avcodec_receive_frame(decode_context, frame_raw);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    std::cerr << "Cannot receive frame." << std::endl;
                }
                if (ret < 0) {
                    std::cerr << "Decoding error." << std::endl;
                }
                if (ret == 0) {
                    if (frame_raw->format == hw_pix_fmt) {
                        // retrieve data from GPU to CPU
                        if ((ret = av_hwframe_transfer_data(frame_yuv, frame_raw, 0)) < 0) {
                            std::cerr << "Cannot transfer HW data to system memory." << std::endl;
                            return -1;
                        }
                    } else {
                        frame_yuv = frame_raw;
                    }

                    // Convert from input format to BGR:
                    cached_ctx = sws_getCachedContext(sws_ctx,
                        decode_context->width,
                        decode_context->height,
                        (AVPixelFormat)frame_yuv->format,
                        decode_context->width,
                        decode_context->height,
                        AV_PIX_FMT_BGR24,
                        SWS_BICUBIC,
                        nullptr,
                        nullptr,
                        nullptr);
                    if (cached_ctx != sws_ctx) {
                        sws_ctx = cached_ctx;
                    }

                    int out_height = sws_scale(sws_ctx,
                        frame_yuv->data,
                        frame_yuv->linesize,
                        0,
                        frame_yuv->height,
                        frame_bgr->data,
                        frame_bgr->linesize);
                    if (out_height != frame_yuv->height) {
                        std::cerr << "Cannot convert image, out height: " << out_height << std::endl;
                        return -1;
                    }

                    // Use OpenCV for showing the image (and save the image in JPEG format):
                    cv::Mat cv_bgr(frame_bgr->height, frame_bgr->width, CV_8UC3, frame_bgr->data[0], frame_bgr->linesize[0]);
                    cv::imshow("preview", cv_bgr);
                    if (cv::waitKey(1) == 27)
                        break;
                }
            }
        }
        av_packet_unref(packet);
    }

    // Clean up
    av_packet_free(&packet);
    sws_freeContext(sws_ctx);
    avcodec_free_context(&decode_context);
    av_frame_free(&frame_raw);
    av_frame_free(&frame_bgr);
    av_frame_free(&frame_yuv);
    avformat_close_input(&format_context);

    return 0;
}