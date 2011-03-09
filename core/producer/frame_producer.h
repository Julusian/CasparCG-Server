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
#pragma once

#include <common/memory/safe_ptr.h>
#include <common/utility/printer.h>

#include <mixer/frame/basic_frame.h>
#include <mixer/frame/frame_factory.h>

#include <boost/noncopyable.hpp>

#include <string>
#include <ostream>

namespace caspar { namespace core {

class frame_producer : boost::noncopyable
{
public:
	virtual ~frame_producer(){}	

	////////////////////////////////////////////////////////////////////////////////////////////////////
	/// \fn	virtual basic_frame :::receive() = 0;
	///
	/// \brief	Renders a frame.
	/// 		
	/// \note	This function is run in through the tbb task_schedular and shall be *non blocking*.
	///
	/// \return	The frame. 
	////////////////////////////////////////////////////////////////////////////////////////////////////
	virtual safe_ptr<basic_frame> receive() = 0;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	/// \fn	virtual std::shared_ptr<frame_producer> :::get_following_producer() const
	///
	/// \brief	Gets the producer which will replace the current producer on EOF. 
	///
	/// \return	The following producer, or nullptr if there is no following producer. 
	////////////////////////////////////////////////////////////////////////////////////////////////////
	virtual safe_ptr<frame_producer> get_following_producer() const {return frame_producer::empty();}  // nothrow
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	/// \fn	virtual void :::set_leading_producer(const std::shared_ptr<frame_producer>& producer)
	///
	/// \brief	Sets the producer which was run before the current producer. 
	///
	/// \param	producer	The leading producer.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	virtual void set_leading_producer(const safe_ptr<frame_producer>& /*producer*/) {}  // nothrow
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	/// \fn	virtual void :::initialize(const safe_ptr<frame_factory>& frame_factory) = 0;
	///
	/// \brief	Provides the frame frame_factory used to create frames and initializes the producer. 
	///
	/// \param	frame_factory	The frame frame_factory. 
	////////////////////////////////////////////////////////////////////////////////////////////////////
	virtual void initialize(const safe_ptr<frame_factory>& frame_factory) = 0;

	virtual void set_parent_printer(const printer& parent_printer) = 0;

	static const safe_ptr<frame_producer>& empty()  // nothrow
	{
		struct empty_frame_producer : public frame_producer
		{
			virtual safe_ptr<basic_frame> receive(){return basic_frame::empty();}
			virtual void initialize(const safe_ptr<frame_factory>&){}
			virtual std::wstring print() const { return L"empty";}
			virtual void set_parent_printer(const printer& parent_printer) {}
		};
		static safe_ptr<frame_producer> producer = make_safe<empty_frame_producer>();
		return producer;
	}

	virtual std::wstring print() const = 0; // nothrow
};

inline std::wostream& operator<<(std::wostream& out, const frame_producer& producer)
{
	out << producer.print().c_str();
	return out;
}

inline std::wostream& operator<<(std::wostream& out, const safe_ptr<const frame_producer>& producer)
{
	out << producer->print().c_str();
	return out;
}

typedef std::function<safe_ptr<core::frame_producer>(const std::vector<std::wstring>&)> producer_factory_t;

void register_producer_factory(const producer_factory_t& factory);
safe_ptr<core::frame_producer> create_producer(const std::vector<std::wstring>& params);


}}
