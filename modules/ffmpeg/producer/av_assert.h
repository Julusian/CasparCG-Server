#pragma once

#include <boost/exception/all.hpp>

#include <exception>
#include <string>

struct exception_t : virtual boost::exception, virtual std::exception {};
struct runtime_error_t : virtual exception_t {};
struct logic_error_t : virtual exception_t {};

namespace caspar {
namespace ffmpeg {

struct averror_bsf_not_found_t : virtual runtime_error_t {};
struct averror_decoder_not_found_t : virtual runtime_error_t {};
struct averror_demuxer_not_found_t : virtual runtime_error_t {};
struct averror_encoder_not_found_t : virtual runtime_error_t {};
struct averror_eof_t : virtual runtime_error_t {};
struct averror_exit_t : virtual runtime_error_t {};
struct averror_filter_not_found_t : virtual runtime_error_t {};
struct averror_muxer_not_found_t : virtual runtime_error_t {};
struct averror_option_not_found_t : virtual runtime_error_t {};
struct averror_patchwelcome_t : virtual logic_error_t {};
struct averror_protocol_not_found_t : virtual runtime_error_t {};
struct averror_stream_not_found_t : virtual runtime_error_t {};

std::string av_error_str(int errn);

void throw_on_ffmpeg_error(int ret,
                           const char* source,
                           const char* func,
                           const char* local_func,
                           const char* file,
                           int line);
void throw_on_ffmpeg_error(int ret,
                           const std::string& source,
                           const char* func,
                           const char* local_func,
                           const char* file,
                           int line);

#define THROW_ON_ERROR_STR_(call) #call
#define THROW_ON_ERROR_STR(call) THROW_ON_ERROR_STR_(call)

#define FF_RET(ret, func)                                              \
  ffmpeg::throw_on_ffmpeg_error(ret, "", func, __FUNCTION__, __FILE__, \
                                __LINE__);

#define FF(call)                                                          \
  [&]() -> int {                                                          \
    auto ret = call;                                                      \
    ffmpeg::throw_on_ffmpeg_error(static_cast<int>(ret), "",              \
                                  THROW_ON_ERROR_STR(call), __FUNCTION__, \
                                  __FILE__, __LINE__);                    \
    return ret;                                                           \
  }()

}  // namespace ffmpeg
}  // namespace caspar