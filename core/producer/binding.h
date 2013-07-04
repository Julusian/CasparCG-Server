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
* Author: Helge Norberg, helge.norberg@svt.se
*/

#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <map>
#include <algorithm>

namespace caspar { namespace core {

namespace detail {

struct impl_base : std::enable_shared_from_this<impl_base>
{
	std::vector<std::shared_ptr<impl_base>> dependencies_;
	mutable std::vector<std::pair<
			std::weak_ptr<void>,
			std::function<void ()>>> on_change_;

	virtual ~impl_base()
	{
	}

	virtual void evaluate() = 0;

	void depend_on(const std::shared_ptr<impl_base>& dependency)
	{
		dependency->on_change(shared_from_this(), [=] { evaluate(); });
		dependencies_.push_back(dependency);
	}

	void on_change(
			const std::weak_ptr<void>& dependant,
			const std::function<void ()>& listener) const
	{
		on_change_.push_back(std::make_pair(dependant, listener));
	}
};

}

template <typename T>
class binding
{
private:

	struct impl : public detail::impl_base
	{
		T value_;
		std::function<T ()> expression_;

		impl()
			: value_()
		{
		}

		impl(T value)
			: value_(value)
		{
		}

		impl(const std::function<T ()>& expression)
			: expression_(expression)
		{
		}

		T get() const
		{
			return value_;
		}

		bool bound() const
		{
			return static_cast<bool>(expression_);
		}

		void set(T value)
		{
			if (bound())
			{
				throw std::exception("Bound value cannot be set");
			}

			if (value == value_)
				return;

			value_ = value;

			on_change();
		}

		void evaluate() override
		{
			if (expression_)
			{
				auto new_value = expression_();

				if (new_value != value_)
				{
					value_ = new_value;
					on_change();
				}
			}
		}

		using impl_base::on_change;
		void on_change()
		{
			auto copy = on_change_;

			for (int i = static_cast<int>(copy.size()) - 1; i >= 0; --i)
			{
				auto strong = copy[i].first.lock();

				if (strong)
					copy[i].second();
				else
					on_change_.erase(on_change_.begin() + i);
			}
		}

		void bind(const std::shared_ptr<impl>& other)
		{
			unbind();
			depend_on(other);
			expression_ = [&]{ return other.get(); }
			evaluate();
		}

		void unbind()
		{
			if (expression_)
			{
				expression_ = std::function<T ()>();
				dependencies_.clear();
			}
		}
	};

	template<typename> friend class binding;

	std::shared_ptr<impl> impl_;
public:
	binding()
		: impl_(new impl)
	{
	}

	explicit binding(T value)
		: impl_(new impl(value))
	{
	}

	template<typename T2>
	binding(const std::function<T ()>& expression, const binding<T2>& dep)
		: impl_(new impl(expression))
	{
		depend_on(dep);
		impl_->evaluate();
	}

	template<typename T2, typename T3>
	binding(
			const std::function<T ()>& expression,
			const binding<T2>& dep1,
			const binding<T3>& dep2)
		: impl_(new impl(expression))
	{
		depend_on(dep1);
		depend_on(dep2);
		impl_->evaluate();
	}

	T get() const
	{
		return impl_->get();
	}

	void set(T value)
	{
		impl_->set(value);
	}

	void bind(const binding<T>& other)
	{
		impl_->bind(other.impl_);
	}

	bool bound() const
	{
		return impl_->bound();
	}

	template<typename T2>
	void depend_on(const binding<T2>& other)
	{
		impl_->depend_on(other.impl_);
	}

	binding<T> operator+(T other) const
	{
		return transformed([other](T self) { return self + other; });
	}

	binding<T> operator+(const binding<T>& other) const
	{
		auto self = impl_;
		auto o = other.impl_;

		return binding<T>(
				[self, o] { return o->get() + self->get(); },
				*this,
				other);
	}

	binding<T> operator-() const
	{
		return transformed([](T self) { return -self; });
	}

	binding<T> operator-(const binding<T>& other) const
	{
		return *this + -other;
	}

	binding<T> operator-(T other) const
	{
		return *this + -other;
	}

	binding<T> operator*(T other) const
	{
		return transformed([other](T self) { return self * other; });
	}

	binding<T> operator*(const binding<T>& other) const
	{
		auto self = impl_;
		auto o = other.impl_;

		return binding<T>(
				[self, o] { return o->get() * self->get(); },
				*this,
				other);
	}

	binding<T> operator/(T other) const
	{
		return transformed([other](T self) { return self / other; });
	}

	binding<T> operator/(const binding<T>& other) const
	{
		auto self = impl_;
		auto o = other.impl_;

		return binding<T>(
				[self, o] { return self->get() / o->get(); },
				*this,
				other);
	}

	binding<bool> operator==(T other) const
	{
		return transformed([other](T self) { return self == other; });
	}

	binding<bool> operator==(const binding<T>& other) const
	{
		auto self = impl_;
		auto o = other.impl_;

		return binding<bool>(
				[self, o] { return self->get() == o->get(); },
				*this,
				other);
	}

	binding<bool> operator!=(T other) const
	{
		return transformed([other](T self) { return self != other; });
	}

	binding<bool> operator!=(const binding<T>& other) const
	{
		auto self = impl_;
		auto o = other.impl_;

		return binding<bool>(
				[self, o] { return self->get() != o->get(); },
				*this,
				other);
	}

	template<typename T2>
	binding<T2> as() const
	{
		return transformed([](T val) { return static_cast<T2>(val); });
	}

	template<typename Func>
	auto transformed(const Func& func) const -> binding<decltype(func(impl_->value_))>
	{
		typedef decltype(func(impl_->value_)) R;
		auto self = impl_;

		return binding<R>(
				[self, func] { return func(self->get()); },
				*this);
	}

	void unbind()
	{
		impl_->unbind();
	}

	void on_change(
			const std::weak_ptr<void> dependant,
			const std::function<void ()>& listener) const
	{
		impl_->on_change(dependant, listener);
	}

	std::shared_ptr<void> on_change(
			const std::function<void ()>& listener) const
	{
		std::shared_ptr<void> subscription(new char);

		on_change(subscription, listener);
		
		return subscription;
	}
private:
	binding(const std::shared_ptr<impl>& self)
		: impl_(self)
	{
	}
};

}}
