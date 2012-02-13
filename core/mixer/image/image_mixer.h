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

#pragma once

#include "blend_modes.h"

#include <common/forward.h>
#include <common/spl/memory.h>

#include <core/video_format.h>
#include <core/frame/frame_visitor.h>

#include <boost/range.hpp>

#include <stdint.h>

FORWARD1(boost, template<typename> class shared_future);
FORWARD2(caspar, core, class data_frame);
FORWARD2(caspar, core, struct pixel_format_desc);

namespace caspar { namespace core {
	
class image_mixer : public frame_visitor
{
public:
	virtual ~image_mixer(){}
	
	virtual void push(const struct frame_transform& frame) = 0;
	virtual void visit(const class data_frame& frame) = 0;
	virtual void pop() = 0;

	virtual void begin_layer(blend_mode blend_mode) = 0;
	virtual void end_layer() = 0;
		
	virtual boost::shared_future<boost::iterator_range<const uint8_t*>> operator()(const struct video_format_desc& format_desc) = 0;
	virtual spl::shared_ptr<core::data_frame> create_frame(const void* tag, const struct pixel_format_desc& desc, double frame_rate, core::field_mode field_mode) = 0;
};

}}