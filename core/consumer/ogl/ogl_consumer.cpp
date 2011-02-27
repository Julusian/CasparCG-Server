/*
* copyright (c) 2010 Sveriges Television AB <info@casparcg.com>
*
*  This file is part of CasparCG.
*
*    CasparCG is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    CasparCG is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.

*    You should have received a copy of the GNU General Public License
*    along with CasparCG.  If not, see <http://www.gnu.org/licenses/>.
*
*/
 
#include "../../StdAfx.h"

#include "ogl_consumer.h"

#include <core/video_format.h>
#include <mixer/frame/read_frame.h>

#include <common/gl/gl_check.h>
#include <common/concurrency/executor.h>
#include <common/memory/safe_ptr.h>
#include <common/diagnostics/graph.h>
#include <common/utility/timer.h>

#include <boost/thread.hpp>

#include <Glee.h>
#include <SFML/Window.hpp>

#include <windows.h>

#include <algorithm>

namespace caspar { namespace core {

struct ogl_consumer::implementation : boost::noncopyable
{		
	printer parent_printer_;
	boost::unique_future<void> active_;

	float wratio_;
	float hratio_;
	
	float wSize_;
	float hSize_;

	GLuint				  texture_;
	std::array<GLuint, 2> pbos_;

	const bool windowed_;
	unsigned int screen_width_;
	unsigned int screen_height_;
	unsigned int screen_x_;
	unsigned int screen_y_;
	const unsigned int screen_index_;
				
	const stretch stretch_;
	video_format_desc format_desc_;
	
	sf::Window window_;
	
	safe_ptr<diagnostics::graph> graph_;
	timer perf_timer_;

	executor executor_;
public:
	implementation(unsigned int screen_index, stretch stretch, bool windowed) 
		: stretch_(stretch)
		, windowed_(windowed)
		, texture_(0)
		, screen_x_(0)
		, screen_y_(0)
		, screen_index_(screen_index)
		, graph_(diagnostics::create_graph(narrow(print())))
		, executor_(print())
	{		
		graph_->guide("frame-time", 0.5);
		graph_->set_color("frame-time", diagnostics::color(1.0f, 0.0f, 0.0f));
	}

	~implementation()
	{
		CASPAR_LOG(info) << print() << L" Shutting down.";	
	}

	void initialize(const video_format_desc& format_desc)
	{
		format_desc_ = format_desc;

		screen_width_ = format_desc.width;
		screen_height_ = format_desc.height;
#ifdef _WIN32
		DISPLAY_DEVICE d_device;			
		memset(&d_device, 0, sizeof(d_device));
		d_device.cb = sizeof(d_device);

		std::vector<DISPLAY_DEVICE> displayDevices;
		for(int n = 0; EnumDisplayDevices(NULL, n, &d_device, NULL); ++n)
		{
			displayDevices.push_back(d_device);
			memset(&d_device, 0, sizeof(d_device));
			d_device.cb = sizeof(d_device);
		}

		if(screen_index_ >= displayDevices.size())
			BOOST_THROW_EXCEPTION(out_of_range() << arg_name_info("screen_index_") << msg_info(narrow(print())));
		
		DEVMODE devmode;
		memset(&devmode, 0, sizeof(devmode));
		
		if(!EnumDisplaySettings(displayDevices[screen_index_].DeviceName, ENUM_CURRENT_SETTINGS, &devmode))
			BOOST_THROW_EXCEPTION(invalid_operation() << arg_name_info("screen_index") << msg_info(narrow(print()) + " EnumDisplaySettings"));
		
		screen_width_ = windowed_ ? format_desc_.width : devmode.dmPelsWidth;
		screen_height_ = windowed_ ? format_desc_.height : devmode.dmPelsHeight;
		screen_x_ = devmode.dmPosition.x;
		screen_y_ = devmode.dmPosition.y;
#else
		if(!windowed)
			BOOST_THROW_EXCEPTION(not_supported() << msg_info(narrow(print() + " doesn't support non-Win32 fullscreen"));

		if(screen_index != 0)
			CASPAR_LOG(warning) << print() << " only supports screen_index=0 for non-Win32";
#endif

		if(!GLEE_ARB_pixel_buffer_object)
			BOOST_THROW_EXCEPTION(not_supported() << msg_info("Missing OpenGL Extension Support: GLEE_ARB_pixel_buffer_object."));

		if(!GLEE_ARB_texture_rectangle)
			BOOST_THROW_EXCEPTION(not_supported() << msg_info("Missing OpenGL Extension Support: GLEE_ARB_texture_rectangle."));

		executor_.start();
		executor_.invoke([=]
		{
			window_.Create(sf::VideoMode(format_desc_.width, format_desc_.height, 32), narrow(print()), windowed_ ? sf::Style::Titlebar : sf::Style::Fullscreen);
			window_.ShowMouseCursor(false);
			window_.SetPosition(screen_x_, screen_y_);
			window_.SetSize(screen_width_, screen_height_);
			window_.SetActive();
			GL(glEnable(GL_TEXTURE_2D));
			GL(glDisable(GL_DEPTH_TEST));		
			GL(glClearColor(0.0, 0.0, 0.0, 0.0));
			GL(glViewport(0, 0, format_desc_.width, format_desc_.height));
			glLoadIdentity();
				
			wratio_ = static_cast<float>(format_desc_.width)/static_cast<float>(format_desc_.width);
			hratio_ = static_cast<float>(format_desc_.height)/static_cast<float>(format_desc_.height);

			std::pair<float, float> target_ratio = None();
			if(stretch_ == fill)
				target_ratio = Fill();
			else if(stretch_ == uniform)
				target_ratio = Uniform();
			else if(stretch_ == uniform_to_fill)
				target_ratio = UniformToFill();

			wSize_ = target_ratio.first;
			hSize_ = target_ratio.second;
			
			glGenTextures(1, &texture_);
			glBindTexture(GL_TEXTURE_2D, texture_);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, format_desc_.width, format_desc_.height, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
			glBindTexture(GL_TEXTURE_2D, 0);
			
			GL(glGenBuffers(2, pbos_.data()));
			
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbos_[0]);
			glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, format_desc_.size, 0, GL_STREAM_DRAW_ARB);
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbos_[1]);
			glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, format_desc_.size, 0, GL_STREAM_DRAW_ARB);
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
		});
		active_ = executor_.begin_invoke([]{});
		CASPAR_LOG(info) << print() << " Sucessfully initialized.";
	}

	void set_parent_printer(const printer& parent_printer)
	{
		parent_printer_ = parent_printer;
	}
	
	std::pair<float, float> None()
	{
		float width = static_cast<float>(format_desc_.width)/static_cast<float>(screen_width_);
		float height = static_cast<float>(format_desc_.height)/static_cast<float>(screen_height_);

		return std::make_pair(width, height);
	}

	std::pair<float, float> Uniform()
	{
		float aspect = static_cast<float>(format_desc_.width)/static_cast<float>(format_desc_.height);
		float width = std::min(1.0f, static_cast<float>(screen_height_)*aspect/static_cast<float>(screen_width_));
		float height = static_cast<float>(screen_width_*width)/static_cast<float>(screen_height_*aspect);

		return std::make_pair(width, height);
	}

	std::pair<float, float> Fill()
	{
		return std::make_pair(1.0f, 1.0f);
	}

	std::pair<float, float> UniformToFill()
	{
		float wr = static_cast<float>(format_desc_.width)/static_cast<float>(screen_width_);
		float hr = static_cast<float>(format_desc_.height)/static_cast<float>(screen_height_);
		float r_inv = 1.0f/std::min(wr, hr);

		float width = wr*r_inv;
		float height = hr*r_inv;

		return std::make_pair(width, height);
	}

	void render(const safe_ptr<const read_frame>& frame)
	{			
		glBindTexture(GL_TEXTURE_2D, texture_);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos_[0]);

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, format_desc_.width, format_desc_.height, GL_BGRA, GL_UNSIGNED_BYTE, 0);

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos_[1]);

		glBufferData(GL_PIXEL_UNPACK_BUFFER, format_desc_.size, 0, GL_STREAM_DRAW);

		auto ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
		if(ptr)
		{
			std::copy_n(frame->image_data().begin(), frame->image_data().size(), reinterpret_cast<char*>(ptr));
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release the mapped buffer
		}

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
				
		GL(glClear(GL_COLOR_BUFFER_BIT));			
		glBegin(GL_QUADS);
				glTexCoord2f(0.0f,	  hratio_);	glVertex2f(-wSize_, -hSize_);
				glTexCoord2f(wratio_, hratio_);	glVertex2f( wSize_, -hSize_);
				glTexCoord2f(wratio_, 0.0f);	glVertex2f( wSize_,  hSize_);
				glTexCoord2f(0.0f,	  0.0f);	glVertex2f(-wSize_,  hSize_);
		glEnd();
		
		glBindTexture(GL_TEXTURE_2D, 0);

		std::rotate(pbos_.begin(), pbos_.begin() + 1, pbos_.end());
	}
		
	void send(const safe_ptr<const read_frame>& frame)
	{
		active_.get();
		active_ = executor_.begin_invoke([=]
		{
			perf_timer_.reset();
			sf::Event e;
			while(window_.GetEvent(e)){}
			render(frame);
			window_.Display();
			graph_->update("frame-time", static_cast<float>(perf_timer_.elapsed()/format_desc_.interval*0.5));
		});
	}

	std::wstring print() const
	{
		return (parent_printer_ ? parent_printer_() + L"/" : L"") + L"ogl[" + boost::lexical_cast<std::wstring>(screen_index_) + L"]";
	}

	size_t buffer_depth() const{return 2;}
};

ogl_consumer::ogl_consumer(ogl_consumer&& other) : impl_(std::move(other.impl_)){}
ogl_consumer::ogl_consumer(unsigned int screen_index, stretch stretch, bool windowed) : impl_(new implementation(screen_index, stretch, windowed)){}
void ogl_consumer::send(const safe_ptr<const read_frame>& frame){impl_->send(frame);}
size_t ogl_consumer::buffer_depth() const{return impl_->buffer_depth();}
void ogl_consumer::initialize(const video_format_desc& format_desc){impl_->initialize(format_desc);}
void ogl_consumer::set_parent_printer(const printer& parent_printer){impl_->set_parent_printer(parent_printer);}

safe_ptr<frame_consumer> create_ogl_consumer(const std::vector<std::wstring>& params)
{
	if(params.size() < 1 || params[0] != L"OGL")
		return frame_consumer::empty();
	
	unsigned int screen_index = 0;
	stretch stretch = stretch::fill;
	bool windowed = true;
	
	try{if(params.size() > 1) screen_index = boost::lexical_cast<int>(params[2]);}
	catch(boost::bad_lexical_cast&){}
	try{if(params.size() > 2) windowed = boost::lexical_cast<bool>(params[3]);}
	catch(boost::bad_lexical_cast&){}

	return make_safe<ogl_consumer>(screen_index, stretch, windowed);
}

}}
