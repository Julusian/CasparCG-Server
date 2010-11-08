#pragma once

#include "packet.h"

#include <system_error>

namespace caspar { namespace core { namespace ffmpeg{	
	
typedef std::shared_ptr<AVFormatContext> AVFormatContextPtr;

class input : boost::noncopyable
{
public:
	input();
	void load(const std::string& filename);
	const std::shared_ptr<AVCodecContext>& get_video_codec_context() const;
	const std::shared_ptr<AVCodecContext>& get_audio_codec_context() const;

	video_packet_ptr get_video_packet();
	audio_packet_ptr get_audio_packet();

	bool seek(unsigned long long frame);
	void start();

	bool is_eof() const;
	void set_loop(bool value);
private:
	struct implementation;
	std::shared_ptr<implementation> impl_;
};
typedef std::shared_ptr<input> input_ptr;
typedef std::unique_ptr<input> input_uptr;

	}
}}
