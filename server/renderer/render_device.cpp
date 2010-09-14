#include "..\StdAfx.h"

#include "render_device.h"
#include "layer.h"

#include "../protocol/monitor/Monitor.h"
#include "../consumer/frame_consumer.h"

#include "../frame/system_frame.h"
#include "../frame/frame_format.h"

#include "../../common/utility/scope_exit.h"
#include "../../common/image/image.h"

#include <numeric>

#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/range/sub_range.hpp>
#include <boost/range/adaptor/indirected.hpp>
#include <boost/foreach.hpp>

#include <tbb/parallel_for.h>
#include <tbb/mutex.h>

using namespace boost::assign;
	
namespace caspar{ namespace renderer{

struct render_device::implementation : boost::noncopyable
{	
	implementation(const caspar::frame_format_desc& format_desc, unsigned int index, const std::vector<frame_consumer_ptr>& consumers)  
		: consumers_(consumers), monitor_(index), fmt_(format_desc)
	{	
		is_running_ = true;
		if(consumers.empty())
			BOOST_THROW_EXCEPTION(invalid_argument() 
									<< arg_name_info("consumer") 
									<< msg_info("render_device requires atleast one consumer"));

		if(std::any_of(consumers.begin(), consumers.end(), [&](const frame_consumer_ptr& pConsumer){ return pConsumer->get_frame_format_desc() != format_desc;}))
			BOOST_THROW_EXCEPTION(invalid_argument() 
									<< arg_name_info("consumer") 
									<< msg_info("All consumers must have same frameformat as renderdevice."));
		
		frame_buffer_.set_capacity(3);
		display_thread_ = boost::thread([=]{display();});
		render_thread_ = boost::thread([=]{render();});

		CASPAR_LOG(info) << L"Initialized render_device with " << format_desc;
	}
			
	~implementation()
	{
		is_running_ = false;
		frame_buffer_.clear();
		frame_buffer_.push(nullptr);
		render_thread_.join();
		display_thread_.join();
	}
	
	void render()
	{		
		CASPAR_LOG(info) << L"Started render_device::Render Thread";
		win32_exception::install_handler();
		
		std::vector<frame_ptr> current_frames;
		std::vector<layer> active_layers;

		while(is_running_)
		{
			try
			{
				{
					tbb::mutex::scoped_lock lock;
					if(lock.try_acquire(layers_mutex_))
					{ // Copy layers into rendering thread
						active_layers.resize(layers_.size());
						boost::range::transform(layers_, active_layers.begin(), boost::bind(&std::map<int, layer>::value_type::second, _1));
					}
				}

				std::vector<frame_ptr> next_frames;
				frame_ptr composite_frame;		
			
				tbb::parallel_invoke(
					[&]{next_frames = render_frames(active_layers);}, 
					[&]{composite_frame = compose_frames(current_frames.empty() ? std::make_shared<system_frame>(fmt_.size) : current_frames[0], current_frames);});

				current_frames = std::move(next_frames);		
				frame_buffer_.push(std::move(composite_frame));
			}
			catch(...)
			{
				CASPAR_LOG_CURRENT_EXCEPTION();
				layers_.clear();
				active_layers.clear();
				CASPAR_LOG(error) << "Unexpected exception. Cleared layers in render-device";
			}
		}

		CASPAR_LOG(info) << L"Ended render_device::Render Thread";
	}

	std::vector<frame_ptr> render_frames(std::vector<layer>& layers)
	{	
		std::vector<frame_ptr> frames(layers.size(), nullptr);
		tbb::parallel_for(tbb::blocked_range<size_t>(0, frames.size()), [&](const tbb::blocked_range<size_t>& r)
		{
			for(size_t i = r.begin(); i != r.end(); ++i)
				frames[i] = layers[i].get_frame();
		});					
		boost::range::remove_erase(frames, nullptr);
		boost::range::remove_erase_if(frames, [](const frame_const_ptr& frame) { return *frame == *frame::null();});
		return frames;
	}
	
	void display()
	{
		CASPAR_LOG(info) << L"Started render_device::Display Thread";
		win32_exception::install_handler();
				
		frame_ptr frame = clear_frame(std::make_shared<system_frame>(fmt_.size));
		std::deque<frame_ptr> prepared(3, frame);
				
		while(is_running_)
		{
			if(!frame_buffer_.try_pop(frame))
			{
				CASPAR_LOG(trace) << "Display Buffer Underrun";
				frame_buffer_.pop(frame);
			}
			send_frame(prepared.front(), frame);
			prepared.push_back(frame);
			prepared.pop_front();
		}
		
		CASPAR_LOG(info) << L"Ended render_device::Display Thread";
	}

	void send_frame(const frame_ptr& pPreparedFrame, const frame_ptr& pNextFrame)
	{
		BOOST_FOREACH(const frame_consumer_ptr& consumer, consumers_)
		{
			try
			{
				consumer->prepare(pNextFrame); // Could block
				consumer->display(pPreparedFrame); // Could block
			}
			catch(...)
			{
				CASPAR_LOG_CURRENT_EXCEPTION();
				boost::range::remove_erase(consumers_, consumer);
				CASPAR_LOG(warning) << "Removed consumer from render-device.";
				if(consumers_.empty())
				{
					CASPAR_LOG(warning) << "No consumers available. Shutting down render-device.";
					is_running_ = false;
				}
			}
		}
	}
		
	void load(int exLayer, const frame_producer_ptr& pProducer, load_option option)
	{
		if(pProducer->get_frame_format_desc() != fmt_)
			BOOST_THROW_EXCEPTION(invalid_argument() << arg_name_info("pProducer") << msg_info("Invalid frame format"));
		tbb::mutex::scoped_lock lock(layers_mutex_);
		layers_[exLayer].load(pProducer, option);
	}
			
	void play(int exLayer)
	{		
		tbb::mutex::scoped_lock lock(layers_mutex_);
		auto it = layers_.find(exLayer);
		if(it != layers_.end())
			it->second.play();		
	}

	void stop(int exLayer)
	{		
		tbb::mutex::scoped_lock lock(layers_mutex_);
		auto it = layers_.find(exLayer);
		if(it != layers_.end())
			it->second.stop();
	}

	void clear(int exLayer)
	{
		tbb::mutex::scoped_lock lock(layers_mutex_);
		auto it = layers_.find(exLayer);
		if(it != layers_.end())
			it->second.clear();		
	}
		
	void clear()
	{
		tbb::mutex::scoped_lock lock(layers_mutex_);
		layers_.clear();
	}		

	frame_producer_ptr active(int exLayer) const
	{
		tbb::mutex::scoped_lock lock(layers_mutex_);
		auto it = layers_.find(exLayer);
		return it != layers_.end() ? it->second.active() : nullptr;
	}
	
	frame_producer_ptr background(int exLayer) const
	{
		tbb::mutex::scoped_lock lock(layers_mutex_);
		auto it = layers_.find(exLayer);
		return it != layers_.end() ? it->second.background() : nullptr;
	}
		
	boost::thread render_thread_;
	boost::thread display_thread_;
		
	caspar::frame_format_desc fmt_;
	tbb::concurrent_bounded_queue<frame_ptr> frame_buffer_;
	
	std::vector<frame_consumer_ptr> consumers_;
	
	mutable tbb::mutex layers_mutex_;
	std::map<int, layer> layers_;
	
	tbb::atomic<bool> is_running_;	

	caspar::Monitor monitor_;
};

render_device::render_device(const caspar::frame_format_desc& format_desc, unsigned int index, const std::vector<frame_consumer_ptr>& consumers) 
	: impl_(new implementation(format_desc, index, consumers)){}
void render_device::load(int exLayer, const frame_producer_ptr& pProducer, load_option option){ impl_->load(exLayer, pProducer, option);}
void render_device::play(int exLayer){ impl_->play(exLayer);}
void render_device::stop(int exLayer){ impl_->stop(exLayer);}
void render_device::clear(int exLayer){ impl_->clear(exLayer);}
void render_device::clear(){ impl_->clear();}
frame_producer_ptr render_device::active(int exLayer) const { return impl_->active(exLayer); }
frame_producer_ptr render_device::background(int exLayer) const { return impl_->background(exLayer); }
const frame_format_desc& render_device::frame_format_desc() const{return impl_->fmt_;}
caspar::Monitor& render_device::monitor(){return impl_->monitor_;}
}}

