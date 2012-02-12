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

#include "frame_producer.h"

#include "../monitor/monitor.h"

#include <common/forward.h>
#include <common/spl/memory.h>

#include <boost/property_tree/ptree_fwd.hpp>

#include <string>

FORWARD1(boost, template<typename T> class unique_future);
FORWARD1(boost, template<typename T> class optional);

namespace caspar { namespace core {
	
class layer sealed : public monitor::observable
{
public:
	layer(int index = -1); // nothrow
	layer(layer&& other); // nothrow
	layer& operator=(layer&& other); // nothrow

	void swap(layer& other); // nothrow 
		
	void load(spl::shared_ptr<class frame_producer> producer, const boost::optional<int32_t>& auto_play_delta = nullptr); // nothrow
	void play(); // nothrow
	void pause(); // nothrow
	void stop(); // nothrow
		
	spl::shared_ptr<class frame_producer> foreground() const; // nothrow
	spl::shared_ptr<class frame_producer> background() const; // nothrow

	spl::shared_ptr<class draw_frame> receive(frame_producer::flags flags, const struct video_format_desc& format_desc); // nothrow

	boost::property_tree::wptree info() const;

	// monitor::observable

	virtual void subscribe(const monitor::observable::observer_ptr& o) override;
	virtual void unsubscribe(const monitor::observable::observer_ptr& o) override;
private:
	struct impl;
	spl::shared_ptr<impl> impl_;
};

}}