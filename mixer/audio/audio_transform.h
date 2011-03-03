#pragma once

namespace caspar { namespace core {

class audio_transform
{
public:
	audio_transform();

	void set_gain(double value);
	double get_gain() const;

	audio_transform& operator*=(const audio_transform &other);
	const audio_transform operator*(const audio_transform &other) const;
private:
	double gain_;
};

audio_transform lerp(const audio_transform& lhs, const audio_transform& rhs, float alpha);

}}