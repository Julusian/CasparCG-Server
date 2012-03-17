/*
* Copyright (c) 2011 Sveriges Television AB <info@casparcg.com>
*
* This file is part of CasparCG (www.casparcg.com).
*
* CasparCG is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* CasparCG is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with CasparCG. If not, see <http://www.gnu.org/licenses/>.
*
* Author: Robert Nagy, ronag89@gmail.com
*/

#include "../../stdafx.h"

#include "input.h"

#include "../util/util.h"
#include "../../ffmpeg_error.h"

#include <common/diagnostics/graph.h>
#include <common/executor.h>
#include <common/except.h>
#include <common/log.h>

#include <core/video_format.h>

#include <tbb/concurrent_queue.h>
#include <tbb/atomic.h>
#include <tbb/recursive_mutex.h>

#include <boost/range/algorithm.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#if defined(_MSC_VER)
#pragma warning (push)
#pragma warning (disable : 4244)
#endif
extern "C" 
{
	#define __STDC_CONSTANT_MACROS
	#define __STDC_LIMIT_MACROS
	#include <libavformat/avformat.h>
}
#if defined(_MSC_VER)
#pragma warning (pop)
#endif

static const size_t MAX_BUFFER_COUNT = 100;
static const size_t MIN_BUFFER_COUNT = 32;
static const size_t MAX_BUFFER_SIZE  = 32 * 1000000;

namespace caspar { namespace ffmpeg {
		
struct input::impl : boost::noncopyable
{		
	const spl::shared_ptr<diagnostics::graph>							graph_;

	const spl::shared_ptr<AVFormatContext>								format_context_; // Destroy this last
	const int													default_stream_index_;
			
	const std::wstring											filename_;
	tbb::atomic<uint32_t>										start_;		
	tbb::atomic<uint32_t>										length_;
	tbb::atomic<bool>											loop_;
	tbb::atomic<bool>											eof_;
	uint32_t													frame_number_;
	
	tbb::concurrent_bounded_queue<std::shared_ptr<AVPacket>>	buffer_;
	tbb::atomic<size_t>											buffer_size_;
		
	executor													executor_;
	
	impl(const spl::shared_ptr<diagnostics::graph> graph, const std::wstring& filename, const bool loop, const uint32_t start, const uint32_t length) 
		: graph_(graph)
		, format_context_(open_input(filename))		
		, default_stream_index_(av_find_default_stream_index(format_context_.get()))
		, filename_(filename)
		, frame_number_(0)
		, executor_(print())
	{		
		start_			= start;
		length_			= length;
		loop_			= loop;
		eof_			= false;
		buffer_size_	= 0;

		if(start_ > 0)			
			seek(start_, false);
								
		graph_->set_color("seek", diagnostics::color(1.0f, 0.5f, 0.0f));	
		graph_->set_color("buffer-count", diagnostics::color(0.7f, 0.4f, 0.4f));
		graph_->set_color("buffer-size", diagnostics::color(1.0f, 1.0f, 0.0f));	

		tick();
	}
	
	bool try_pop(std::shared_ptr<AVPacket>& packet)
	{
		auto result = buffer_.try_pop(packet);
		
		if(result)
		{
			if(packet)
				buffer_size_ -= packet->size;
			tick();
		}

		graph_->set_value("buffer-size", (static_cast<double>(buffer_size_)+0.001)/MAX_BUFFER_SIZE);
		graph_->set_value("buffer-count", (static_cast<double>(buffer_.size()+0.001)/MAX_BUFFER_COUNT));
		
		return result;
	}

	void seek(uint32_t target, bool clear)
	{
		executor_.invoke([=]
		{
			if(clear)
			{
				std::shared_ptr<AVPacket> packet;
				while(buffer_.try_pop(packet) && packet)
					buffer_size_ -= packet->size;
			}
			
			CASPAR_LOG(debug) << print() << " Seeking: " << target;

			int flags = AVSEEK_FLAG_FRAME;
			if(target == 0)
			{
				// Fix VP6 seeking
				int vid_stream_index = av_find_best_stream(format_context_.get(), AVMEDIA_TYPE_VIDEO, -1, -1, 0, 0);
				if(vid_stream_index >= 0)
				{
					auto codec_id = format_context_->streams[vid_stream_index]->codec->codec_id;
					if(codec_id == CODEC_ID_VP6A || codec_id == CODEC_ID_VP6F || codec_id == CODEC_ID_VP6)
						flags = AVSEEK_FLAG_BYTE;
				}
			}
		
			auto stream = format_context_->streams[default_stream_index_];
			auto codec  = stream->codec;
			auto fixed_target = (target*stream->time_base.den*codec->time_base.num)/(stream->time_base.num*codec->time_base.den)*codec->ticks_per_frame;
		
			THROW_ON_ERROR2(avformat_seek_file(format_context_.get(), default_stream_index_, std::numeric_limits<int64_t>::min(), fixed_target, std::numeric_limits<int64_t>::max(), 0), print());		
		
			auto flush_packet	= create_packet();
			flush_packet->data	= nullptr;
			flush_packet->size	= 0;
			flush_packet->pos	= target;

			buffer_.push(flush_packet);
			
			tick();
		}, task_priority::high_priority);
	}
	
	std::wstring print() const
	{
		return L"ffmpeg_input[" + filename_ + L")]";
	}
	
	bool full() const
	{
		return (buffer_size_ > MAX_BUFFER_SIZE || buffer_.size() > MAX_BUFFER_COUNT) && buffer_.size() > MIN_BUFFER_COUNT;
	}

	void tick()
	{	
		if(!executor_.is_running())
			return;
		
		executor_.begin_invoke([this]
		{			
			if(full())
				return;

			try
			{
				auto packet = create_packet();
		
				auto ret = av_read_frame(format_context_.get(), packet.get()); // packet is only valid until next call of av_read_frame. Use av_dup_packet to extend its life.	
		
				if(is_eof(ret))														     
				{
					frame_number_ = 0;

					if(loop_)
					{
						seek(start_, false);
						graph_->set_tag("seek");		
						CASPAR_LOG(trace) << print() << " Looping.";	
					}
					else
						eof_ = true;
				}
				else
				{		
					eof_ = false;

					THROW_ON_ERROR(ret, "av_read_frame", print());

					if(packet->stream_index == default_stream_index_)
						++frame_number_;

					THROW_ON_ERROR2(av_dup_packet(packet.get()), print());
				
					// Make sure that the packet is correctly deallocated even if size and data is modified during decoding.
					auto size = packet->size;
					auto data = packet->data;
			
					packet = spl::shared_ptr<AVPacket>(packet.get(), [packet, size, data](AVPacket*)
					{
						packet->size = size;
						packet->data = data;				
					});

					buffer_.try_push(packet);
					buffer_size_ += packet->size;
				
					graph_->set_value("buffer-size", (static_cast<double>(buffer_size_)+0.001)/MAX_BUFFER_SIZE);
					graph_->set_value("buffer-count", (static_cast<double>(buffer_.size()+0.001)/MAX_BUFFER_COUNT));		
				}	

				if(!eof_)
					tick();		
			}
			catch(...)
			{
				CASPAR_LOG_CURRENT_EXCEPTION();
				executor_.stop();
			}
		});
	}	
			
	bool is_eof(int ret)
	{
		#pragma warning (disable : 4146)

		//if(ret == AVERROR(EIO))
		//	CASPAR_LOG(trace) << print() << " Received EIO, assuming EOF. ";
		//if(ret == AVERROR_EOF)
		//	CASPAR_LOG(trace) << print() << " Received EOF. ";

		return ret == AVERROR_EOF || ret == AVERROR(EIO) || frame_number_ >= length_; // av_read_frame doesn't always correctly return AVERROR_EOF;
	}
};

input::input(const spl::shared_ptr<diagnostics::graph>& graph, const std::wstring& filename, bool loop, uint32_t start, uint32_t length) 
	: impl_(new impl(graph, filename, loop, start, length)){}
bool input::eof() const {return impl_->eof_;}
bool input::try_pop(std::shared_ptr<AVPacket>& packet){return impl_->try_pop(packet);}
spl::shared_ptr<AVFormatContext> input::context(){return impl_->format_context_;}
void input::loop(bool value){impl_->loop_ = value;}
bool input::loop() const{return impl_->loop_;}
void input::seek(uint32_t target){impl_->seek(target, true);}
void input::start(uint32_t value){impl_->start_ = value;}
uint32_t input::start() const{return impl_->start_;}
void input::length(uint32_t value){impl_->length_ = value;}
uint32_t input::length() const{return impl_->length_;}
}}
