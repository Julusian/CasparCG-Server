#include "../stdafx.h"

#include "graph.h"

#pragma warning (disable : 4244)

#include "../concurrency/executor.h"
#include "../utility/timer.h"
#include "../env.h"

#include <SFML/Graphics.hpp>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/range/algorithm_ext/erase.hpp>

#include <numeric>
#include <map>
#include <array>

namespace caspar { namespace diagnostics {
		
struct drawable : public sf::Drawable
{
	virtual ~drawable(){}
	virtual void render(sf::RenderTarget& target) = 0;
	virtual void Render(sf::RenderTarget& target) const { const_cast<drawable*>(this)->render(target);}
};

class context : public drawable
{	
	timer timer_;
	sf::RenderWindow window_;
	
	std::list<std::shared_ptr<drawable>> drawables_;
		
	executor executor_;
public:					

	template<typename Func>
	static auto begin_invoke(Func&& func) -> boost::unique_future<decltype(func())> // noexcept
	{	
		return get_instance().executor_.begin_invoke(std::forward<Func>(func));	
	}

	static void register_drawable(const std::shared_ptr<drawable>& drawable)
	{
		if(!drawable)
			return;

		begin_invoke([=]
		{
			get_instance().do_register_drawable(drawable);
		});
	}
			
private:
	context()
	{
		executor_.start();
		executor_.begin_invoke([this]
		{
			window_.Create(sf::VideoMode(600, 1000), "CasparCG Diagnostics");
			window_.SetPosition(0, 0);
			window_.SetActive();
			glEnable(GL_BLEND);
			glEnable(GL_LINE_SMOOTH);
			glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			tick();
		});
	}

	void tick()
	{
		sf::Event e;
		while(window_.GetEvent(e)){}		
		glClear(GL_COLOR_BUFFER_BIT);
		window_.Draw(*this);
		window_.Display();
		timer_.tick(1.0/50.0);
		executor_.begin_invoke([this]{tick();});
	}

	void render(sf::RenderTarget& target)
	{
		auto count = std::max<size_t>(10, drawables_.size());
		float target_dy = 1.0f/static_cast<float>(count);

		float last_y = 0.0f;
		int n = 0;
		for(auto it = drawables_.begin(); it != drawables_.end(); ++n)
		{
			auto& drawable = *it;
			if(!drawable.unique())
			{
				drawable->SetScale(static_cast<float>(window_.GetWidth()), static_cast<float>(target_dy*window_.GetHeight()));
				float target_y = std::max(last_y, static_cast<float>(n * window_.GetHeight())*target_dy);
				drawable->SetPosition(0.0f, target_y);			
				target.Draw(*drawable);				
				++it;		
			}
			else	
				it = drawables_.erase(it);			
		}		
	}	
	
	void do_register_drawable(const std::shared_ptr<drawable>& drawable)
	{
		drawables_.push_back(drawable);
	}
	
	static context& get_instance()
	{
		static context impl;
		return impl;
	}
};

class guide : public drawable
{
	float value_;
	color c_;
public:
	guide(color c = color(1.0f, 1.0f, 1.0f, 0.6f)) 
		: value_(0.0f)
		, c_(c){}

	guide(float value, color c = color(1.0f, 1.0f, 1.0f, 0.6f)) 
		: value_(value)
		, c_(c){}
			
	void set_color(color c) {c_ = c;}

	void render(sf::RenderTarget&)
	{		
		glEnable(GL_LINE_STIPPLE);
		glLineStipple(3, 0xAAAA);
		glBegin(GL_LINE_STRIP);	
			glColor4f(c_.red, c_.green, c_.blue+0.2f, c_.alpha);		
			glVertex3f(0.0f, (1.0f-value_) * 0.8f + 0.1f, 0.0f);		
			glVertex3f(1.0f, (1.0f-value_) * 0.8f + 0.1f, 0.0f);	
		glEnd();
		glDisable(GL_LINE_STIPPLE);
	}
};

class line : public drawable
{
	boost::optional<diagnostics::guide> guide_;
	boost::circular_buffer<std::pair<float, bool>> line_data_;

	std::vector<float>		tick_data_;
	bool					tick_tag_;
	color c_;
public:
	line(size_t res = 600)
		: line_data_(res)
		, tick_tag_(false)
		, c_(1.0f, 1.0f, 1.0f)
	{
		line_data_.push_back(std::make_pair(-1.0f, false));
	}
	
	void update(float value)
	{
		tick_data_.push_back(value);
	}

	void set(float value)
	{
		tick_data_.clear();
		tick_data_.push_back(value);
	}
	
	void tag()
	{
		tick_tag_ = true;
	}

	void guide(const guide& guide)
	{
		guide_ = guide;
		guide_->set_color(c_);
	}
	
	void set_color(color c)
	{
		c_ = c;
		if(guide_)
			guide_->set_color(c_);
	}

	color get_color() const { return c_; }
	
	void render(sf::RenderTarget& target)
	{
		float dx = 1.0f/static_cast<float>(line_data_.capacity());
		float x = static_cast<float>(line_data_.capacity()-line_data_.size())*dx;

		if(!tick_data_.empty())
		{
			float sum = std::accumulate(tick_data_.begin(), tick_data_.end(), 0.0) + std::numeric_limits<float>::min();
			line_data_.push_back(std::make_pair(static_cast<float>(sum)/static_cast<float>(tick_data_.size()), tick_tag_));
			tick_data_.clear();
		}
		else if(!line_data_.empty())
		{
			line_data_.push_back(std::make_pair(line_data_.back().first, tick_tag_));
		}
		tick_tag_ = false;

		if(guide_)
			target.Draw(*guide_);
		
		glBegin(GL_LINE_STRIP);
		glColor4f(c_.red, c_.green, c_.blue, 1.0f);		
		for(size_t n = 0; n < line_data_.size(); ++n)		
			if(line_data_[n].first > -0.5)
				glVertex3f(x+n*dx, std::max(0.05f, std::min(0.95f, (1.0f-line_data_[n].first)*0.8f + 0.1f)), 0.0f);		
		glEnd();
				
		glEnable(GL_LINE_STIPPLE);
		glLineStipple(3, 0xAAAA);
		for(size_t n = 0; n < line_data_.size(); ++n)	
		{
			if(line_data_[n].second)
			{
				glBegin(GL_LINE_STRIP);
				glColor4f(c_.red, c_.green, c_.blue, c_.alpha);					
					glVertex3f(x+n*dx, 0.0f, 0.0f);				
					glVertex3f(x+n*dx, 1.0f, 0.0f);		
				glEnd();
			}
		}
		glDisable(GL_LINE_STIPPLE);
	}
};

struct graph::implementation : public drawable
{
	std::map<std::string, diagnostics::line> lines_;
	std::string name_;

	implementation(const std::string& name) 
		: name_(name){}

	void update(const std::string& name, float value)
	{
		context::begin_invoke([=]
		{
			lines_[name].update(value);
		});
	}

	void set(const std::string& name, float value)
	{
		context::begin_invoke([=]
		{
			lines_[name].set(value);
		});
	}

	void tag(const std::string& name)
	{
		context::begin_invoke([=]
		{
			lines_[name].tag();
		});
	}

	void set_color(const std::string& name, color c)
	{
		context::begin_invoke([=]
		{
			lines_[name].set_color(c);
		});
	}
	
	void guide(const std::string& name, float value)
	{
		context::begin_invoke([=]
		{
			lines_[name].guide(diagnostics::guide(value));
		});
	}

private:
	void render(sf::RenderTarget& target)
	{
		const size_t text_size = 15;
		const size_t text_margin = 2;
		const size_t text_offset = text_size+text_margin*2;

		sf::String text(name_.c_str(), sf::Font::GetDefaultFont(), text_size);
		text.SetStyle(sf::String::Italic);
		text.Move(text_margin, text_margin);
		
		glPushMatrix();
			glScaled(1.0f/GetScale().x, 1.0f/GetScale().y, 1.0f);
			target.Draw(text);
			float x_offset = text.GetPosition().x + text.GetRect().Right + text_margin*4;
			for(auto it = lines_.begin(); it != lines_.end(); ++it)
			{						
				sf::String line_text(it->first, sf::Font::GetDefaultFont(), text_size);
				line_text.SetPosition(x_offset, text_margin);
				auto c = it->second.get_color();
				line_text.SetColor(sf::Color(c.red*255.0f, c.green*255.0f, c.blue*255.0f, c.alpha*255.0f));
				target.Draw(line_text);
				x_offset = line_text.GetRect().Right + text_margin*2;
			}

			glDisable(GL_TEXTURE_2D);
		glPopMatrix();

		glBegin(GL_QUADS);
			glColor4f(1.0f, 1.0f, 1.0f, 0.2f);	
			glVertex2f(1.0f, 0.99f);
			glVertex2f(0.0f, 0.99f);
			glVertex2f(0.0f, 0.01f);	
			glVertex2f(1.0f, 0.01f);	
		glEnd();

		glPushMatrix();
			glTranslated(0.0f, text_offset/GetScale().y, 1.0f);
			glScaled(1.0f, 1.0-text_offset/GetScale().y, 1.0f);
		
			target.Draw(diagnostics::guide(1.0f, color(1.0f, 1.0f, 1.0f, 0.6f)));
			target.Draw(diagnostics::guide(0.0f, color(1.0f, 1.0f, 1.0f, 0.6f)));

			for(auto it = lines_.begin(); it != lines_.end(); ++it)		
				target.Draw(it->second);
		
		glPopMatrix();
	}

	implementation(implementation&);
	implementation& operator=(implementation&);
};
	
graph::graph(const std::string& name) : impl_(env::properties().get("configuration.diagnostics.graphs", true) ? new implementation(name) : nullptr)
{
	if(impl_)
		context::register_drawable(impl_);
}

void graph::update(const std::string& name, float value){if(impl_)impl_->update(name, value);}
void graph::set(const std::string& name, float value){if(impl_)impl_->set(name, value);}
void graph::tag(const std::string& name){if(impl_)impl_->tag(name);}
void graph::guide(const std::string& name, float value){if(impl_)impl_->guide(name, value);}
void graph::set_color(const std::string& name, color c){if(impl_)impl_->set_color(name, c);}

safe_ptr<graph> create_graph(const std::string& name)
{
	return safe_ptr<graph>(new graph(name));
}

}}