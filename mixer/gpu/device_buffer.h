#pragma once

#include "../gpu/host_buffer.h"

#include <boost/noncopyable.hpp>

#include <memory>

namespace caspar { namespace mixer {
	
class device_buffer : boost::noncopyable
{
public:
	device_buffer(size_t width, size_t height, size_t stride);
	
	size_t stride() const;	
	size_t width() const;
	size_t height() const;
		
	void bind();
	void unbind();

	void attach(int index);
	void read(host_buffer& source);
	void write(host_buffer& target);
	
private:
	struct implementation;
	std::shared_ptr<implementation> impl_;
};

}}