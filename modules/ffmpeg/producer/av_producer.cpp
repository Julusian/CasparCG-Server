#include <exception>
#include <memory>
#include <string>

#include <boost/exception/exception.hpp>
#include <boost/format.hpp>
#include <boost/range/algorithm_ext/rotate.hpp>

#include <common/scope_exit.h>
#include <core/frame/draw_frame.h>
#include <core/frame/frame.h>
#include <core/frame/frame_factory.h>
#include <core/help/help_repository.h>
#include <core/help/help_sink.h>
#include <core/producer/framerate/framerate_producer.h>
#include <core/producer/media_info/media_info.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timecode.h>
}

#include "av_assert.h"

namespace caspar {
namespace ffmpeg {

core::pixel_format get_pixel_format(AVPixelFormat pix_fmt) {
  switch (pix_fmt) {
    case AVPixelFormat::AV_PIX_FMT_GRAY8:
      return core::pixel_format::gray;
    case AVPixelFormat::AV_PIX_FMT_RGB24:
      return core::pixel_format::rgb;
    case AVPixelFormat::AV_PIX_FMT_BGR24:
      return core::pixel_format::bgr;
    case AVPixelFormat::AV_PIX_FMT_BGRA:
      return core::pixel_format::bgra;
    case AVPixelFormat::AV_PIX_FMT_ARGB:
      return core::pixel_format::argb;
    case AVPixelFormat::AV_PIX_FMT_RGBA:
      return core::pixel_format::rgba;
    case AVPixelFormat::AV_PIX_FMT_ABGR:
      return core::pixel_format::abgr;
    case AVPixelFormat::AV_PIX_FMT_YUV444P:
      return core::pixel_format::ycbcr;
    case AVPixelFormat::AV_PIX_FMT_YUV422P:
      return core::pixel_format::ycbcr;
    case AVPixelFormat::AV_PIX_FMT_YUV420P:
      return core::pixel_format::ycbcr;
    case AVPixelFormat::AV_PIX_FMT_YUV411P:
      return core::pixel_format::ycbcr;
    case AVPixelFormat::AV_PIX_FMT_YUV410P:
      return core::pixel_format::ycbcr;
    case AVPixelFormat::AV_PIX_FMT_YUVA420P:
      return core::pixel_format::ycbcra;
    default:
      return core::pixel_format::invalid;
  }
}

core::pixel_format_desc pixel_format_desc(AVPixelFormat pix_fmt,
                                          int width,
                                          int height) {
  // Get linesizes
  AVPicture dummy_pict;
  avpicture_fill(&dummy_pict, nullptr, pix_fmt, width, height);

  core::pixel_format_desc desc = get_pixel_format(pix_fmt);

  switch (desc.format) {
    case core::pixel_format::gray:
    case core::pixel_format::luma: {
      desc.planes.push_back(
          core::pixel_format_desc::plane(dummy_pict.linesize[0], height, 1));
      return desc;
    }
    case core::pixel_format::bgr:
    case core::pixel_format::rgb: {
      desc.planes.push_back(core::pixel_format_desc::plane(
          dummy_pict.linesize[0] / 3, height, 3));
      return desc;
    }
    case core::pixel_format::bgra:
    case core::pixel_format::argb:
    case core::pixel_format::rgba:
    case core::pixel_format::abgr: {
      desc.planes.push_back(core::pixel_format_desc::plane(
          dummy_pict.linesize[0] / 4, height, 4));
      return desc;
    }
    case core::pixel_format::ycbcr:
    case core::pixel_format::ycbcra: {
      // Find chroma height
      int size2 = static_cast<int>(dummy_pict.data[2] - dummy_pict.data[1]);
      int h2 = size2 / dummy_pict.linesize[1];

      desc.planes.push_back(
          core::pixel_format_desc::plane(dummy_pict.linesize[0], height, 1));
      desc.planes.push_back(
          core::pixel_format_desc::plane(dummy_pict.linesize[1], h2, 1));
      desc.planes.push_back(
          core::pixel_format_desc::plane(dummy_pict.linesize[2], h2, 1));

      if (desc.format == core::pixel_format::ycbcra)
        desc.planes.push_back(
            core::pixel_format_desc::plane(dummy_pict.linesize[3], height, 1));
      return desc;
    }
    default:
      desc.format = core::pixel_format::invalid;
      return desc;
  }
}

struct stream_t {
  std::shared_ptr<AVCodecContext> dec;

  std::shared_ptr<AVFilterGraph> graph;
  AVFilterContext* source = nullptr;
  AVFilterContext* sink = nullptr

      std::queue<std::shared_ptr<AVFrame>>
          buffer;
};
struct av_producer_t::impl {
  const std::shared_ptr<core::frame_factory> frame_factory_;
  const std::string filename_;

  std::vector<int> audio_cadence_ std::vector<uint32_t> audio_buffer_;
  core::audio_channel_layout channel_layout_;

  std::shared_ptr<AVFormatContext> ctx_;
  int video_stream_index_;
  int audio_stream_index_;
  boost::optional<stream_t> video_stream_;
  boost::optional<stream_t> audio_stream_;

  impl(const std::shared_ptr<core::frame_factory>& frame_factory,
       const core::video_format_desc& format_desc,
       const std::string& filename,
       const std::string& vfilter,
       const std::string& afilter)
      : frame_factory_(frame_factory),
        audio_cadence_(format_desc.audio_cadence)
        // TODO
        ,
        channel_layout_(),
        filename_(filename),
        input_(filename_) {
    AVFormatContext* raw_ctx;
    FF(avformat_open_input(&raw_ctx, filename.c_str(), NULL, NULL));
    ctx_ = std::shared_ptr<AVFormatContext>(
        raw_ctx, [](AVFormatContext* ctx) { avformat_close_input(&ctx); });

    FF(avformat_find_stream_info(ctx_.get()));

    video_stream_index_ =
        av_find_best_stream(ctx_.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    audio_stream_index_ =
        av_find_best_stream(ctx_.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    const auto open_stream = [&](AVStream* st, std::string filter_spec) {
      char args[4096];

      auto dec = avcodec_find_decoder(st->codecpar->codec_id);

      // REQUIRE(dec);

      auto codec_ctx = std::shared_ptr<AVCodecContext>(
          avcodec_alloc_context3(dec),
          [](AVCodecContext* ptr) { avcodec_free_context(&ptr); });

      // REQUIRE(codec_ctx);

      FF(avcodec_parameters_to_context(codec_ctx, st->codecpar));

      // REQUIRE(codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO ||
      // codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO);

      if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
        codec_ctx->framerate = av_guess_frame_rate(ctx, st, NULL);

      FF(avcodec_open2(codec_ctx.get(), dec, NULL));

      const auto dec_ctx = stream.dec;

      AVFilterContext* buffersrc_ctx = nullptr;
      AVFilterContext* buffersink_ctx = nullptr;
      AVFilterInOut* outputs = avfilter_inout_alloc();
      AVFilterInOut* inputs = avfilter_inout_alloc();

      if (!outputs || !inputs)
        FF_RET(AVERROR(ENOMEM), "avfilter_inout_alloc");

      CASPAR_SCOPE_EXIT {
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
      };

      auto filter_graph = std::shared_ptr<AVFilterGraph>(
          avfilter_graph_alloc(),
          [](AVFilterGraph* ptr) { avfilter_graph_free(&ptr); });

      if (!filter_graph)
        FF_RET(AVERROR(ENOMEM), "avfilter_graph_alloc");

      if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        if (filter_spec.empty())
          filter_spec = "null";

        // TODO format_desc.sar
        if (format_desc.field_mode == = core::field_mode::progressive)
          filter_spec += ",yadif=1:-1:1";

        snprintf(args, sizeof(args), "scale=%d:%d,fps=%d/%d", format_desc.width,
                 format_desc.height, format_desc.framerate.num(),
                 format_desc.framerate.den());

        // TODO 50p => 50i

        filter_spec += args;

        const auto buffersrc = avfilter_get_by_name("buffer");
        const auto buffersink = avfilter_get_by_name("buffersink");
        if (!buffersrc || !buffersink)
          FF_RET(AVERROR(ENOMEM), "avfilter_get_by_name");

        snprintf(
            args, sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
            dec_ctx->time_base.num, dec_ctx->time_base.den,
            dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);

        FF(avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL, filter_graph.get());
        FF(avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL, NULL, filter_graph.get()));

        const uint8_t[] pix_fmt = {
            AV_PIX_FMT_GRAY8,
            AV_PIX_FMT_RGB24,
            AV_PIX_FMT_BGR24,
            AV_PIX_FMT_BGRA,
            AV_PIX_FMT_ARGB,
            AV_PIX_FMT_RGBA,
            AV_PIX_FMT_ABGR,
            AV_PIX_FMT_YUV444P,
            AV_PIX_FMT_YUV422P,
            AV_PIX_FMT_YUV420P,
            AV_PIX_FMT_YUV411P,
            AV_PIX_FMT_YUV410P,
            AV_PIX_FMT_YUVA420P
        };

        FF(av_opt_set_bin(buffersink_ctx, "pix_fmts", pix_fmt, sizeof(pix_fmt), AV_OPT_SEARCH_CHILDREN));
      } else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        if (filter_spec.empty())
          filter_spec = "anull";

        filter_spec += ",aresample=async=1000";

        const auto buffersrc = avfilter_get_by_name("abuffer");
        const auto buffersink = avfilter_get_by_name("abuffersink");
        if (!buffersrc || !buffersink)
          FF_RET(AVERROR(ENOMEM), "avfilter_get_by_name");

        if (!dec_ctx->channel_layout)
          dec_ctx->channel_layout =
              av_get_default_channel_layout(dec_ctx->channels);

        snprintf(args, sizeof(args),
                 "time_base=%d/"
                 "%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%" PRIx64,
                 dec_ctx->time_base.num, dec_ctx->time_base.den,
                 dec_ctx->sample_rate,
                 av_get_sample_fmt_name(dec_ctx->sample_fmt),
                 dec_ctx->channel_layout);

        FF(avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args,
                                        NULL, filter_graph.get()));
        FF(avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                        NULL, NULL, filter_graph.get()));

        const uint8_t sample_fmt = AV_SAMPLE_FMT_S32;
        // TODO channel_layout
        const uint8_t channel_layout =
            av_get_default_channel_layout(channel_layout_.num_channels);
        const uint8_t sample_rate = format_desc.audio_sample_rate;

        FF(av_opt_set_bin(buffersink_ctx, "sample_fmts", &sample_fmt,
                          sizeof(sample_fmt), AV_OPT_SEARCH_CHILDREN));
        FF(av_opt_set_bin(buffersink_ctx, "channel_layouts", &channel_layout,
                          sizeof(channel_layout), AV_OPT_SEARCH_CHILDREN));
        FF(av_opt_set_bin(buffersink_ctx, "sample_rates", &sample_rate,
                          sizeof(sample_rate), AV_OPT_SEARCH_CHILDREN));
      } else {
        FF_RET(AVERROR(AVERROR_UNKNOWN), "invalid codec type");
      }

      outputs->name = av_strdup("in");
      outputs->filter_ctx = buffersrc_ctx;
      outputs->pad_idx = 0;
      outputs->next = NULL;

      inputs->name = av_strdup("out");
      inputs->filter_ctx = buffersink_ctx;
      inputs->pad_idx = 0;
      inputs->next = NULL;

      if (!outputs->name || !inputs->name)
        FF_RET(AVERROR(ENOMEM), "av_strdup");

            FF(avfilter_graph_parse_ptr(filter_graph, filter_spec, &inputs, &outputs, NULL)));
            FF(avfilter_graph_config(filter_graph, NULL)));

            stream_t stream;
            stream.source = buffersrc_ctx;
            stream.sink = buffersink_ctx;
            stream.graph = filter_graph;
            stream.dec = codec_ctx;
            return stream;
    };

    if (video_stream_index_ >= 0)
      video_stream_ = open_stream(ctx_->streams[video_stream_index_];, vfilter);

    if (audio_stream_index_ >= 0)
      audio_stream_ = open_stream(ctx_->streams[audio_stream_index_];, afilter);

    // av_dump_format(ctx_.get(), 0, filename, 0);
  }

  bool read() {
    int ret;

    AVPacket packet = {};

    ret = av_read_frame(ctx_.get(), &packet);

    if (ret == AVERROR_EOF)
      return false;

    FF_RET(ret, "av_read_frame");

    CASPAR_SCOPE_EXIT { av_packet_unref(&packet); };

    boost::optional<stream_t> stream;

    if (packet->stream_index == = video_stream_index_)
      stream = video_stream_;
    else if (packet->stream_index == = audio_stream_index_)
      stream = audio_stream_;

    if (!stream)
      return true;

    FF(avcodec_send_packet(stream->dec.get(), &packet));

    while (true) {
      auto frame = std::shared_ptr<AVFrame>(
          av_frame_alloc(), [](AVFrame* ptr) { av_frame_free(ptr); });

      ret = avcodec_receive_frame(stream->dec.get(), frame.get());
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        break;

      FF_RET(ret, "avcodec_receive_frame");

      CASPAR_SCOPE_EXIT { av_frame_unref(frame.get()); };

      frame->pts = frame->best_effort_timestamp;

      FF(av_buffersrc_add_frame_flags(stream->source, frame.get(),
                                      AV_BUFFERSRC_FLAG_KEEP_REF));

      while (true) {
        auto filt_frame = std::shared_ptr<AVFrame>(
            av_frame_alloc(), [](AVFrame* ptr) { av_frame_free(ptr); });

        ret = av_buffersink_get_frame(stream->sink, filt_frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
          break;

        FF_RET(ret, "av_buffersink_get_frame");

        stream->buffer.push(std::shared_ptr<AVFrame>(
            filt_frame.get(),
            [filt_frame](AVFrame* ptr) { av_frame_unref(ptr); }));
      }
    }

    return true;
  }

  std::shared_ptr<AVFrame> read_frame(stream_t& stream) {
    while (stream.buffer.empty()) {
      if (!read())
        return nullptr;
    }

    const frame = stream.buffer.front();
    stream.buffer.pop();
    return frame;
  }

  core::draw_frame next() {
    std::shared_ptr<AVFrame> video =
        video_stream_ ? read_frame(*video_stream_) : nullptr;
    std::shared_ptr<AVFrame> audio =
        audio_stream_ ? read_frame(*audio_stream_) : nullptr;

    if (!video && !audio)
      return core::draw_frame::eof();

    core::mutable_frame frame;

    if (video) {
      auto desc = pixel_format_desc(static_cast<AVPixelFormat>(video->format),
                                    video->width, video->height);

      frame = frame_factory.create_frame(this, desc, channel_layout_);

      for (int n = 0; n < static_cast<int>(desc.planes.size()); ++n) {
        for (int y = 0; y < desc.planes[n].height; ++y)
          std::memcpy(write.image_data(n).begin() + y * desc.planes[n].linesize,
                      video->data[n] + y * video->linesize[n],
                      desc.planes[n].linesize);
      }
    } else {
      frame = frame_factory_->create_frame(this, core::pixel_format::invalid,
                                           channel_layout_);
    }

    if (audio) {
      {
        auto beg = reinterpret_cast<uint32_t*>(audio->data[0]);
        auto end = beg + audio->linesize[0] / sizeof(uin32_t);
        audio_buffer_.insert(audio_buffer_.end(), beg, end);
      }

      {
        auto beg = audio_buffer_.begin();
        auto end = beg + audio_cadence_.front() * channel_layout_.num_channels;
        frame.audio_data() = core::mutable_audio_buffer(begin, end);
        audio_buffer_.erase(begin, end);
      }
    } else {
      frame.audio_data() = core::mutable_audio_buffer(
          audio_cadence_.front() * channel_layout_.num_channels);
    }

    boost::range::rotate(audio_cadence_, std::begin(audio_cadence_) + 1);

    return std::move(frame);
  }
};

av_producer_t::av_producer_t(
    const std::shared_ptr<core::frame_factory>& frame_factory,
    const core::video_format_desc& format_desc,
    const std::string& filename,
    const std::string& vfilter,
    const std::string& afilter)
    : impl_(new impl{frame_factor, format_desc, filename, vfilter, afilter}) {}

core::draw_frame next() {
  return impl_->next();
}

}  // namespace ffmpeg
}  // namespace caspar