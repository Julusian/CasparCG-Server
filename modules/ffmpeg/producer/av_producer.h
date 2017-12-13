#include <memory>

#include <core/frame/frame.h>
#include <core/frame/frame_factory.h>

namespace caspar {
namespace ffmpeg {

class av_producer_t {
 public:
  av_producer_t(const std::shared_ptr<core::frame_factory>& frame_factory,
                const core::video_format_desc& format_desc,
                const std::string& filename,
                const std::string& vfilter,
                const std::string& afilter);

  core::draw_frame next();

 private:
  struct impl;
  std::unique_ptr<impl> impl_;
}

}  // namespace ffmpeg
}  // namespace caspar