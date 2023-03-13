// This is a C++ demo code of generating glitched video file with FFMPEG.
// For more: https://github.com/yinguobing/make-it-glitch

#include <iostream>
#include <random>
#include <string>

#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

int main(int argc, char** argv)
{
    char* filename = argv[1];

    // Find out the format
    AVFormatContext* format_context = nullptr;
    int ret;
    if ((ret = avformat_open_input(&format_context, filename, NULL, NULL)) < 0) {
        std::cerr << "Cannot open input file" << std::endl;
        return ret;
    }
    if ((ret = avformat_find_stream_info(format_context, NULL)) < 0) {
        std::cerr << "Cannot find stream information" << std::endl;
        return ret;
    }
    std::cout << "Number of streams: " << format_context->nb_streams << std::endl;
    av_dump_format(format_context, 0, filename, 0);

    // Find the best stream
    AVStream* stream;
    int stream_index;
    stream_index = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (stream_index < 0) {
        std::cerr << "Could not find stream " << av_get_media_type_string(AVMEDIA_TYPE_VIDEO) << std::endl;
        return stream_index;
    } else {
        stream = format_context->streams[stream_index];
    }

    // Find the decoder
    AVCodec* decoder;
    decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!decoder) {
        std::cerr << "Failed to find decoder for stream " << std::endl;
        return AVERROR_DECODER_NOT_FOUND;
    }

    // Init the decoder context
    AVCodecContext* decode_context;
    decode_context = avcodec_alloc_context3(decoder);
    if (!decode_context) {
        std::cerr << "Failed to allocate the decoder context for stream " << std::endl;
        return AVERROR(ENOMEM);
    }
    ret = avcodec_parameters_to_context(decode_context, stream->codecpar);
    if (ret < 0) {
        std::cout << "Failed to copy decoder parameters to input decoder context for stream " << std::endl;
        return ret;
    }
    decode_context->framerate = av_guess_frame_rate(format_context, stream, NULL);
    ret = avcodec_open2(decode_context, decoder, NULL);
    if (ret < 0) {
        std::cerr << "Failed to open decoder for stream " << stream_index << std::endl;
        return ret;
    }

    // Create SWS Context for converting from decode pixel format (like YUV420) to BGR
    SwsContext* sws_ctx = nullptr;
    sws_ctx = sws_getContext(decode_context->width,
        decode_context->height,
        AV_PIX_FMT_YUV420P,
        decode_context->width,
        decode_context->height,
        AV_PIX_FMT_BGR24,
        SWS_BICUBIC,
        NULL,
        NULL,
        NULL);
    if (sws_ctx == nullptr) {
        std::cerr << "SWS context init error." << std::endl;
        return -1;
    }

    // Init the packet
    AVPacket* packet = nullptr;
    packet = av_packet_alloc();
    if (!packet) {
        std::cerr << "Failed to alloc packet." << std::endl;
        return -1;
    }

    // Allocate frame for the decoded YUV image
    AVFrame* yuv = av_frame_alloc();
    if (!yuv) {
        std::cerr << "Could not allocate video frame." << std::endl;
        return -1;
    }

    // Allocate frame for storing RGB image
    AVFrame* bgr = av_frame_alloc();
    if (!bgr) {
        std::cerr << "Could not allocate BGR frame." << std::endl;
        return -1;
    }
    bgr->format = AV_PIX_FMT_BGR24;
    bgr->width = decode_context->width;
    bgr->height = decode_context->height;
    if (av_frame_get_buffer(bgr, 0) < 0) {
        std::cerr << "get SWS frame error." << std::endl;
        return -1;
    }

    // Standard mersenne_twister_engine seeded with rd()
    std::random_device rd;
    std::mt19937 gen(rd());

    while (av_read_frame(format_context, packet) >= 0) {
        if (packet->stream_index == stream_index) {
            std::uniform_int_distribution<> distrib(0, packet->size);
            packet->data[distrib(gen)] = 0;

            ret = avcodec_send_packet(decode_context, packet);
            if (ret < 0) {
                std::cerr << "Error submitting a packet for decoding. ERR: " << ret << std::endl;
                continue;
            }

            ret = avcodec_receive_frame(decode_context, yuv);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                std::cerr << "avcodec_receive_frame error." << std::endl;
            }
            if (ret < 0) {
                std::cerr << "Error during decoding." << std::endl;
            }
            if (ret == 0) {
                // Convert from input format (e.g YUV420) to BGR:
                int out_height = sws_scale(sws_ctx,
                    yuv->data,
                    yuv->linesize,
                    0,
                    yuv->height,
                    bgr->data,
                    bgr->linesize);
                if (out_height != yuv->height) {
                    std::cerr << "sws scale error." << std::endl;
                    return -1;
                }

                // Use OpenCV for showing the image (and save the image in JPEG format):
                cv::Mat cv_bgr(bgr->height, bgr->width, CV_8UC3, bgr->data[0], bgr->linesize[0]);
                cv::imshow("preview", cv_bgr);
                if (cv::waitKey(1) == 27)
                    break;
            }
        }
        av_packet_unref(packet);
    }

    // Clean up
    avcodec_free_context(&decode_context);
    av_frame_free(&yuv);
    av_frame_free(&bgr);
    av_packet_free(&packet);
    avformat_close_input(&format_context);
    sws_freeContext(sws_ctx);

    return 0;
}