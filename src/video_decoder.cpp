#include "video_decoder.hpp"

AVPixelFormat VideoDecoder::hw_pix_fmt;

VideoDecoder::VideoDecoder(const std::string url, AVHWDeviceType hw_acc)
{
    // Init the flags
    this->initialized = true;

    // Is this file valid?
    if (avformat_open_input(&ctx_format, url.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "Cannot open input file:" << url << std::endl;
        initialized &= false;
    }

    // Is there any valid video stream to be processed?
    if (avformat_find_stream_info(ctx_format, nullptr) < 0) {
        std::cerr << "Cannot find stream information." << std::endl;
        initialized &= false;
    }
    if ((stream_index = av_find_best_stream(ctx_format, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0)) < 0) {
        std::cerr << "Cannot find valid stream: " << av_get_media_type_string(AVMEDIA_TYPE_VIDEO) << std::endl;
        initialized &= false;
    } else {
        stream = ctx_format->streams[stream_index];
        std::cout << "Found video stream with index: " << stream_index << std::endl;
    }

    // Is there a valid decoder for the target media?
    if (!decoder) {
        std::cerr << "Cannot find decoder for stream: " << av_get_media_type_string(AVMEDIA_TYPE_VIDEO) << std::endl;
        initialized &= false;
    } else {
        std::cout << "Found video decoder: " << decoder->long_name << std::endl;
    }

    // Is this hardware acceleration available?
    query_supported_hw_devices(hw_accelerators);
    if (hw_acc != AV_HWDEVICE_TYPE_NONE) {
        if (std::find(hw_accelerators.begin(), hw_accelerators.end(), hw_acc) == hw_accelerators.end()) {
            std::cerr << "Hardware acceleration not available: " << av_hwdevice_get_type_name(hw_acc) << std::endl;
        } else {
            // Is this decoder supported by the hardware?
            for (int i = 0;; i++) {
                const AVCodecHWConfig* config = avcodec_get_hw_config(decoder, i);
                if (!config) {
                    std::cerr << "Decoder not supported by device: " << av_hwdevice_get_type_name(hw_acc) << std::endl;
                }
                if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == hw_acc) {
                    hw_pix_fmt = config->pix_fmt;
                    this->enabled_hw_accelerator = hw_acc;
                    std::cout << "Acceleration pixel format: " << av_get_pix_fmt_name(hw_pix_fmt) << std::endl;
                    break;
                }
            }
        }
    }

    // Now it's time to init the decoder.
    this->ctx_decode = avcodec_alloc_context3(decoder);
    if (!ctx_decode) {
        std::cerr << "Cannot allocate decoder context." << std::endl;
        initialized &= false;
    }
    if (avcodec_parameters_to_context(ctx_decode, stream->codecpar) < 0) {
        std::cerr << "Cannot copy decoder parameters to input decoder context." << std::endl;
        initialized &= false;
    }

    if (enabled_hw_accelerator != AV_HWDEVICE_TYPE_NONE) {
        if (av_hwdevice_ctx_create(&hw_device_ctx, enabled_hw_accelerator, nullptr, nullptr, 0) == 0) {
            ctx_decode->hw_device_ctx = av_buffer_ref(hw_device_ctx);
            ctx_decode->get_format = get_hw_format;
            this->hw_acc_enabled = true;
            std::cerr << "Hardware accelerated device: " << av_hwdevice_get_type_name(enabled_hw_accelerator) << std::endl;
        } else {
            enabled_hw_accelerator = AV_HWDEVICE_TYPE_NONE;
            hw_pix_fmt = AV_PIX_FMT_NONE;
            std::cerr << "Cannot create context for specified hardware device." << std::endl;
        }
    }
    if (avcodec_open2(ctx_decode, decoder, nullptr) < 0) {
        std::cerr << "Cannot open decoder for stream: " << stream_index << std::endl;
        initialized &= false;
    }

    // Init the frames
    if (!(frame = av_frame_alloc()) or !(frame_hw = av_frame_alloc()) or !(frame_bgr = av_frame_alloc())) {
        std::cerr << "Cannot allocate frame." << std::endl;
        initialized &= false;
    }

    // Init the packet
    if (!(packet = av_packet_alloc())) {
        std::cerr << "Cannot allocate packet." << std::endl;
        initialized &= false;
    }

    // Create SWS Context for converting from decode pixel format (like YUV420) to BGR
    ctx_sws = sws_getContext(ctx_decode->width,
        ctx_decode->height,
        ctx_decode->pix_fmt,
        ctx_decode->width,
        ctx_decode->height,
        this->output_fmt,
        SWS_BICUBIC,
        nullptr,
        nullptr,
        nullptr);
    if (ctx_sws == nullptr) {
        std::cerr << "Cannot init SWS context." << std::endl;
        initialized &= false;
    }
    frame_bgr->format = this->output_fmt;
    frame_bgr->width = ctx_decode->width;
    frame_bgr->height = ctx_decode->height;
    if (av_frame_get_buffer(frame_bgr, 0) < 0) {
        std::cerr << "Cannot allocate SWS frame buffer." << std::endl;
        initialized &= false;
    }
}

VideoDecoder::~VideoDecoder()
{
    if (packet)
        av_packet_free(&packet);
    if (frame)
        av_frame_free(&frame);
    if (frame_hw)
        av_frame_free(&frame_hw);
    if (ctx_decode)
        avcodec_free_context(&ctx_decode);
    if (ctx_format)
        avformat_close_input(&ctx_format);
    if (ctx_sws)
        sws_freeContext(ctx_sws);
}
void VideoDecoder::query_supported_hw_devices(std::vector<AVHWDeviceType>& types)
{
    AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
    while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
        types.push_back(type);
}

AVPixelFormat VideoDecoder::get_hw_format(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts)
{
    const enum AVPixelFormat* p;
    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt)
            return *p;
    }
    std::cerr << "Failed to get HW surface format." << std::endl;
    return AV_PIX_FMT_NONE;
}

void VideoDecoder::random_touch()
{
    // Standard mersenne_twister_engine seeded with rd()
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> corruption_count(1, 6);
    std::uniform_int_distribution<> start(0, packet->size - 1);
    std::uniform_int_distribution<> length(1, 256);
    std::uniform_int_distribution<> val(0, 255);
    int random_start = start(gen);
    for (size_t i = 0, count = corruption_count(gen); i < count; i++) {
        for (int j = 0, random_length = length(gen); j < random_length; j++) {
            if (random_start + j >= packet->size) {
                break;
            }
            packet->data[random_start + j] = val(gen);
        }
    }
}

int VideoDecoder::to_bgr()
{
    ctx_sws = sws_getCachedContext(ctx_sws,
        ctx_decode->width,
        ctx_decode->height,
        (AVPixelFormat)frame->format,
        ctx_decode->width,
        ctx_decode->height,
        output_fmt,
        SWS_BICUBIC,
        nullptr,
        nullptr,
        nullptr);
    int out_height = sws_scale(ctx_sws,
        frame->data,
        frame->linesize,
        0,
        frame->height,
        frame_bgr->data,
        frame_bgr->linesize);
    if (out_height != frame_bgr->height) {
        std::cerr << "Cannot convert image, out height: " << out_height << std::endl;
        return -1;
    }
    return 0;
}

std::vector<std::string> VideoDecoder::list_hw_accelerators()
{
    std::vector<std::string> acc_names;
    for (auto&& acc : this->hw_accelerators) {
        acc_names.push_back(av_hwdevice_get_type_name(acc));
    }
    return acc_names;
}

bool VideoDecoder::is_valid()
{
    return initialized;
}

bool VideoDecoder::is_accelerated()
{
    return initialized and hw_acc_enabled;
}

std::pair<int, int> VideoDecoder::get_frame_dims()
{
    std::pair<int, int> dims;
    dims.first = this->ctx_decode->width;
    dims.second = this->ctx_decode->height;
    return dims;
}
int VideoDecoder::get_frame_steps()
{
    return frame_bgr->linesize[0];
}

uint8_t* VideoDecoder::get_buffer()
{
    if (initialized)
        return frame_bgr->data[0];
    else
        return nullptr;
}

int VideoDecoder::read(bool touch)
{
    int ret = 0;

    // Fetch a frame
    ret = av_read_frame(ctx_format, packet);

    // Is this a video stream?
    if (ret >= 0 and packet->stream_index != stream_index) {
        av_packet_unref(packet);
        return ret;
    }

    // Should the packet be touched?
    if (touch)
        this->random_touch();

    // Try sending the packet.
    if (ret < 0)
        ret = avcodec_send_packet(ctx_decode, NULL);
    else {
        ret = avcodec_send_packet(ctx_decode, packet);
    }
    av_packet_unref(packet);
    if (ret < 0 and touch == false) {
        std::cerr << "Error submitting a packet for decoding: " << ret << std::endl;
        return ret;
    }

    // Frame got?
    if (hw_acc_enabled)
        ret = avcodec_receive_frame(ctx_decode, frame_hw);
    else
        ret = avcodec_receive_frame(ctx_decode, frame);
    if (ret == AVERROR_EOF)
        return -1;
    else if (ret == AVERROR(EAGAIN)) {
        return ret;
    } else if (ret < 0) {
        std::cerr << "Error decoding frame." << ret << std::endl;
        return ret;
    }

    // Retrieve data from GPU to CPU if necessary
    if (hw_acc_enabled) {
        ret = av_hwframe_transfer_data(frame, frame_hw, 0);
        if (ret < 0) {
            std::cerr << "Cannot transfer HW data to system memory." << std::endl;
            return ret;
        }
    }

    // Convert
    ret = to_bgr();

    return ret;
}