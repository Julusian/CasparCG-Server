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

#include "image_consumer.h"

#include <common/except.h>
#include <common/env.h>
#include <common/log.h>
#include <common/utf.h>
#include <common/array.h>
#include <common/future.h>

#include <core/consumer/frame_consumer.h>
#include <core/video_format.h>
#include <core/frame/frame.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>

#include <tbb/concurrent_queue.h>

#include <asmlib.h>

#include <FreeImage.h>

#include <vector>

namespace caspar { namespace image {
	
struct image_consumer : public core::frame_consumer
{
	std::wstring filename_;
public:

	// frame_consumer

	image_consumer(const std::wstring& filename)
		: filename_(filename)
	{
	}

	void initialize(const core::video_format_desc&, int) override
	{
	}
	
	boost::unique_future<bool> send(core::const_frame frame) override
	{
		auto filename = filename_;

		boost::thread async([frame, filename]
		{
			try
			{
				auto filename2 = u8(filename);
				
				if (filename2.empty())
					filename2 = u8(env::media_folder()) +  boost::posix_time::to_iso_string(boost::posix_time::second_clock::local_time()) + ".png";
				else
					filename2 = u8(env::media_folder()) + filename2 + ".png";

				auto bitmap = std::shared_ptr<FIBITMAP>(FreeImage_Allocate(static_cast<int>(frame.width()), static_cast<int>(frame.height()), 32), FreeImage_Unload);
				A_memcpy(FreeImage_GetBits(bitmap.get()), frame.image_data().begin(), frame.image_data().size());
				FreeImage_FlipVertical(bitmap.get());
				FreeImage_Save(FIF_PNG, bitmap.get(), filename2.c_str(), 0);
			}
			catch(...)
			{
				CASPAR_LOG_CURRENT_EXCEPTION();
			}
		});
		async.detach();

		return wrap_as_future(false);
	}

	std::wstring print() const override
	{
		return L"image[]";
	}
	
	std::wstring name() const override
	{
		return L"image";
	}

	boost::property_tree::wptree info() const override
	{
		boost::property_tree::wptree info;
		info.add(L"type", L"image");
		return info;
	}

	int buffer_depth() const override
	{
		return 0;
	}

	int index() const override
	{
		return 100;
	}

	void subscribe(const monitor::observable::observer_ptr& o) override
	{
	}

	void unsubscribe(const monitor::observable::observer_ptr& o) override
	{
	}	
};

spl::shared_ptr<core::frame_consumer> create_consumer(const std::vector<std::wstring>& params)
{
	if (params.size() < 1 || params[0] != L"IMAGE")
		return core::frame_consumer::empty();

	std::wstring filename;

	if (params.size() > 1)
		filename = params[1];

	return spl::make_shared<image_consumer>(filename);
}

}}
