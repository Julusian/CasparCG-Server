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
* Author: Niklas P Andersson, niklas.p.andersson@svt.se
*/

#pragma once

#include "util/bigendian_file_input_stream.h"
#include "misc.h"
#include "layer.h"

#include <boost/property_tree/ptree.hpp>

#include <string>
#include <vector>

namespace caspar { namespace psd {

class psd_document
{
public:
	psd_document();

	std::vector<layer_ptr>& layers()
	{
		return layers_;
	}
	std::uint32_t width() const
	{
		return width_;
	}
	std::uint32_t height() const
	{
		return height_;
	}

	psd::color_mode color_mode() const
	{
		return color_mode_;
	}

	std::uint16_t color_depth() const
	{
		return depth_;
	}
	std::uint16_t channels_count() const
	{
		return channels_;
	}
	const std::wstring& filename() const
	{
		return filename_;
	}
	bool has_timeline() const
	{
		return !timeline_desc_.empty();
	}
	const boost::property_tree::wptree& timeline() const 
	{
		return timeline_desc_;
	}


	void parse(const std::wstring& s);

private:
	void read_header();
	void read_color_mode();
	void read_image_resources();
	void read_layers();

	std::wstring					filename_;
	bigendian_file_input_stream		input_;

	std::vector<layer_ptr>			layers_;

	std::uint16_t					channels_;
	std::uint32_t					width_;
	std::uint32_t					height_;
	std::uint16_t					depth_;
	psd::color_mode					color_mode_;
	boost::property_tree::wptree	timeline_desc_;
};

}	//namespace psd
}	//namespace caspar
