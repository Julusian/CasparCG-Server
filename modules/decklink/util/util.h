#pragma once

#include <core/video_format.h>

#include "../interop/DeckLinkAPI_h.h"

namespace caspar { 
	
static BMDDisplayMode GetDecklinkVideoFormat(core::video_format::type fmt) 
{
	switch(fmt)
	{
	case core::video_format::pal:			return bmdModePAL;
	case core::video_format::ntsc:			return bmdModeNTSC;
	case core::video_format::x576p2500:		return (BMDDisplayMode)ULONG_MAX;
	case core::video_format::x720p2500:		return (BMDDisplayMode)ULONG_MAX;
	case core::video_format::x720p5000:		return bmdModeHD720p50;
	case core::video_format::x720p5994:		return bmdModeHD720p5994;
	case core::video_format::x720p6000:		return bmdModeHD720p60;
	case core::video_format::x1080p2397:	return bmdModeHD1080p2398;
	case core::video_format::x1080p2400:	return bmdModeHD1080p24;
	case core::video_format::x1080i5000:	return bmdModeHD1080i50;
	case core::video_format::x1080i5994:	return bmdModeHD1080i5994;
	case core::video_format::x1080i6000:	return bmdModeHD1080i6000;
	case core::video_format::x1080p2500:	return bmdModeHD1080p25;
	case core::video_format::x1080p2997:	return bmdModeHD1080p2997;
	case core::video_format::x1080p3000:	return bmdModeHD1080p30;
	default:						return (BMDDisplayMode)ULONG_MAX;
	}
}
	
static IDeckLinkDisplayMode* get_display_mode(IDeckLinkOutput* output, BMDDisplayMode format)
{
	IDeckLinkDisplayModeIterator*		iterator;
	IDeckLinkDisplayMode*				mode;
	
	if(FAILED(output->GetDisplayModeIterator(&iterator)))
		return nullptr;

	while(SUCCEEDED(iterator->Next(&mode)) && mode != nullptr)
	{	
		if(mode->GetDisplayMode() == format)
			return mode;
	}
	iterator->Release();
	return nullptr;
}

static IDeckLinkDisplayMode* get_display_mode(IDeckLinkOutput* output, core::video_format::type fmt)
{
	return get_display_mode(output, GetDecklinkVideoFormat(fmt));
}

template<typename T>
static std::wstring get_version(T& deckLinkIterator)
{
	IDeckLinkAPIInformation* deckLinkAPIInformation;
	if (FAILED(deckLinkIterator->QueryInterface(IID_IDeckLinkAPIInformation, (void**)&deckLinkAPIInformation)))
		return L"Unknown";
	
	BSTR ver;		
	deckLinkAPIInformation->GetString(BMDDeckLinkAPIVersion, &ver);
	deckLinkAPIInformation->Release();	
		
	return ver;					
}

}