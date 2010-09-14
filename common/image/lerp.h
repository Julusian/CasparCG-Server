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

#include "../hardware/cpuid.h"

namespace caspar{ namespace common{ namespace image{
		
void lerp_SSE2(void* dest, const void* source1, const void* source2, float alpha, size_t size);
void lerp_REF (void* dest, const void* source1, const void* source2, float alpha, size_t size);
void lerpParallel_SSE2(void* dest, const void* source1, const void* source2, float alpha, size_t size);
void lerpParallel_REF (void* dest, const void* source1, const void* source2, float alpha, size_t size);
void lerp_OLD (void* dest, const void* source1, const void* source2, float alpha, size_t size);

typedef void(*lerp_fun)(void*, const void*, const void*, float, size_t);
lerp_fun get_lerp_fun(SIMD simd = REF);

}}}


