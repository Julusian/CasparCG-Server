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

#include "../StdAfx.h"

#include "ffmpeg_producer.h"

#include "../ffmpeg.h"
#include "../ffmpeg_error.h"

#include <common/param.h>
#include <common/diagnostics/graph.h>
#include <common/future.h>

#include <core/frame/draw_frame.h>
#include <core/help/help_repository.h>
#include <core/help/help_sink.h>
#include <core/producer/media_info/media_info.h>
#include <core/producer/framerate/framerate_producer.h>
#include <core/frame/frame_factory.h>

#include <future>
#include <queue>

namespace caspar { namespace ffmpeg {

struct ffmpeg_producer : public core::frame_producer_base
{
	core::monitor::subject  					monitor_subject_;
	core::draw_frame							last_frame_;
	av_producer_t								impl_;
	const std::shared_ptr<diagnostics::graph>	graph_;
	timer										frame_timer_;
public:
	explicit ffmpeg_producer(
			const spl::shared_ptr<core::frame_factory>& frame_factory,
			const core::video_format_desc& format_desc,
			const std::wstring& filename,
			const std::wstring& vfilter,
			const std::wstring& afilter,
			bool loop,
			uint32_t in,
			uint32_t out,
			bool thumbnail_mode,
			const std::wstring& custom_channel_order,
			const ffmpeg_options& vid_params)
		: last_frame_(core::draw_frame::empty())
		, impl_(frame_factory, format_desc, filename, vfilter, afilter, loop)
	{
		diagnostics::register_graph(graph_);
		graph_->set_color("frame-time", diagnostics::color(0.1f, 1.0f, 0.1f));
		graph_->set_color("underflow", diagnostics::color(0.6f, 0.3f, 0.9f));
	}

	// frame_producer

	core::draw_frame receive_impl() override
	{
	}

	core::draw_frame last_frame() override
	{
		return core::draw_frame::still(last_frame_);
	}

	core::constraints& pixel_constraints() override
	{
	}

	uint32_t nb_frames() const override
	{
	}

	std::future<std::wstring> call(const std::vector<std::wstring>& params) override
	{
	}

	std::wstring print() const override
	{
		return L"ffmpeg";
		// return L"ffmpeg[" + (is_url() ? filename_ : boost::filesystem::path(filename_).filename().wstring()) + L"|"
		// 				  + print_mode() + L"|"
		// 				  + boost::lexical_cast<std::wstring>(file_frame_number_) + L"/" + boost::lexical_cast<std::wstring>(file_nb_frames()) + L"]";
	}

	std::wstring name() const override
	{
		return L"ffmpeg";
	}

	boost::property_tree::wptree info() const override
	{
		boost::property_tree::wptree info;
		return info;
	}

	core::monitor::subject& monitor_output()
	{
		return monitor_subject_;
	}
};

spl::shared_ptr<core::frame_producer> create_producer(
		const core::frame_producer_dependencies& dependencies,
		const std::vector<std::wstring>& params,
		const spl::shared_ptr<core::media_info_repository>& info_repo)
{
	// auto file_or_url	= params.at(0);

	// if (!boost::contains(file_or_url, L"://"))
	// {
	// 	// File
	// 	file_or_url = probe_stem(env::media_folder() + L"/" + file_or_url, false);
	// }

	// if (file_or_url.empty())
	// 	return core::frame_producer::empty();

	// constexpr auto uint32_max = std::numeric_limits<uint32_t>::max();

	// auto loop					= contains_param(L"LOOP",		params);

	// auto in						= get_param(L"SEEK",			params, static_cast<uint32_t>(0)); // compatibility
	// in							= get_param(L"IN",				params, in);

	// auto out					= get_param(L"LENGTH",			params, uint32_max);
	// if (out < uint32_max - in)
	// 	out += in;
	// else
	// 	out = uint32_max;
	// out							= get_param(L"OUT",				params, out);

	// auto filter_str				= get_param(L"FILTER",			params, L"");
	// auto custom_channel_order	= get_param(L"CHANNEL_LAYOUT",	params, L"");

	// boost::ireplace_all(filter_str, L"DEINTERLACE_BOB",	L"YADIF=1:-1");
	// boost::ireplace_all(filter_str, L"DEINTERLACE_LQ",	L"SEPARATEFIELDS");
	// boost::ireplace_all(filter_str, L"DEINTERLACE",		L"YADIF=0:-1");

	// ffmpeg_options vid_params;
	// bool haveFFMPEGStartIndicator = false;
	// for (size_t i = 0; i < params.size() - 1; ++i)
	// {
	// 	if (!haveFFMPEGStartIndicator && params[i] == L"--")
	// 	{
	// 		haveFFMPEGStartIndicator = true;
	// 		continue;
	// 	}
	// 	if (haveFFMPEGStartIndicator)
	// 	{
	// 		auto name = u8(params.at(i++)).substr(1);
	// 		auto value = u8(params.at(i));
	// 		vid_params.push_back(std::make_pair(name, value));
	// 	}
	// }

	// auto producer = spl::make_shared<ffmpeg_producer>(
	// 		dependencies.frame_factory,
	// 		dependencies.format_desc,
	// 		file_or_url,
	// 		filter_str,
	// 		loop,
	// 		in,
	// 		out,
	// 		false,
	// 		custom_channel_order,
	// 		vid_params);

	// if (producer->audio_only())
	// 	return core::create_destroy_proxy(producer);

	// auto get_source_framerate	= [=] { return producer->get_out_framerate(); };
	// auto target_framerate		= dependencies.format_desc.framerate;

	// return core::create_destroy_proxy(core::create_framerate_producer(
	// 		producer,
	// 		get_source_framerate,
	// 		target_framerate,
	// 		dependencies.format_desc.field_mode,
	// 		dependencies.format_desc.audio_cadence));
}

core::draw_frame create_thumbnail_frame(
		const core::frame_producer_dependencies& dependencies,
		const std::wstring& media_file,
		const spl::shared_ptr<core::media_info_repository>& info_repo)
{
	return core::frame::empty();
}

void describe_producer(core::help_sink& sink, const core::help_repository& repo)
{
	sink.short_description(L"A producer for playing media files supported by FFmpeg.");
	sink.syntax(L"[clip,url:string] {[loop:LOOP]} {IN,SEEK [in:int]} {OUT [out:int] | LENGTH [length:int]} {FILTER [filter:string]} {CHANNEL_LAYOUT [channel_layout:string]}");
	sink.para()
		->text(L"The FFmpeg Producer can play all media that FFmpeg can play, which includes many ")
		->text(L"QuickTime video codec such as Animation, PNG, PhotoJPEG, MotionJPEG, as well as ")
		->text(L"H.264, FLV, WMV and several audio codecs as well as uncompressed audio.");
	sink.definitions()
		->item(L"clip", L"The file without the file extension to play. It should reside under the media folder.")
		->item(L"url", L"If clip contains :// it is instead treated as the URL parameter. The URL can either be any streaming protocol supported by FFmpeg, dshow://video={webcam_name} or v4l2://{video device} or iec61883://{auto}.")
		->item(L"loop", L"Will cause the media file to loop between in and out.")
		->item(L"in", L"Optionally sets the first frame. 0 by default. If loop is specified, this will be the frame where it starts over again.")
		->item(L"out", L"Optionally sets the last frame. If not specified the clip will be played to the end. If loop is specified, the file will jump to start position once it reaches the last frame.")
		->item(L"length", L"Optionally sets the length of the clip. Equivalent to OUT in + length.")
		->item(L"filter", L"If specified, will be used as an FFmpeg video filter.")
		->item(L"channel_layout",
				L"Optionally override the automatically deduced audio channel layout."
				L"Either a named layout as specified in casparcg.config or in the format [type:string]:[channel_order:string] for a custom layout.");
	sink.para()->text(L"Examples:");
	sink.example(L">> PLAY 1-10 folder/clip", L"to play all frames in a clip and stop at the last frame.");
	sink.example(L">> PLAY 1-10 folder/clip LOOP", L"to loop a clip between the first frame and the last frame.");
	sink.example(L">> PLAY 1-10 folder/clip LOOP IN 10", L"to loop a clip between frame 10 and the last frame.");
	sink.example(L">> PLAY 1-10 folder/clip LOOP IN 10 LENGTH 50", L"to loop a clip between frame 10 and frame 60.");
	sink.example(L">> PLAY 1-10 folder/clip IN 10 OUT 60", L"to play frames 10-60 in a clip and stop.");
	sink.example(L">> PLAY 1-10 folder/clip FILTER yadif=1,-1", L"to deinterlace the video.");
	sink.example(L">> PLAY 1-10 folder/clip CHANNEL_LAYOUT film", L"given the defaults in casparcg.config this will specifies that the clip has 6 audio channels of the type 5.1 and that they are in the order FL FC FR BL BR LFE regardless of what ffmpeg says.");
	sink.example(L">> PLAY 1-10 folder/clip CHANNEL_LAYOUT \"5.1:LFE FL FC FR BL BR\"", L"specifies that the clip has 6 audio channels of the type 5.1 and that they are in the specified order regardless of what ffmpeg says.");
	sink.example(L">> PLAY 1-10 rtmp://example.com/live/stream", L"to play an RTMP stream.");
	sink.example(L">> PLAY 1-10 \"dshow://video=Live! Cam Chat HD VF0790\"", L"to use a web camera as video input on Windows.");
	sink.example(L">> PLAY 1-10 v4l2:///dev/video0", L"to use a web camera as video input on Linux.");
	sink.example(L">> PLAY 1-10 iec61883://auto", L"to use a FireWire (H)DV video device as video input on Linux.");
	sink.para()->text(L"The FFmpeg producer also supports changing some of the settings via ")->code(L"CALL")->text(L":");
	sink.example(L">> CALL 1-10 LOOP 1");
	sink.example(L">> CALL 1-10 IN 10");
	sink.example(L">> CALL 1-10 OUT 60");
	sink.example(L">> CALL 1-10 LENGTH 50");
	sink.example(L">> CALL 1-10 SEEK 30");
	core::describe_framerate_producer(sink);
}

}}
