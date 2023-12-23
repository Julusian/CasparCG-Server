/*
 * Copyright (c) 2018 Norsk rikskringkasting AS
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
 * Author: Julian Waller, julian@superfly.tv
 */

#include "AMCPCommandsImplTimecode.h"
#include "amcp_command_context.h"

#include <core/producer/frame_producer.h>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

namespace caspar { namespace protocol { namespace amcp {

std::wstring time_command(command_context& ctx)
{
    const auto ch = ctx.channel.raw_channel->stage()->timecode();

    if (!ctx.parameters.empty()) {
        if (!ch->is_free())
            return L"403 TIME FAILED\r\n";

        core::frame_timecode tc;
        const uint8_t fps = static_cast<uint8_t>(round(ctx.channel.raw_channel->stage()->video_format_desc().fps));
        if (!core::frame_timecode::parse_string(ctx.parameters.at(0), fps, true, tc))
            return L"403 TIME FAILED\r\n";

        ch->timecode(tc);
    }

    std::wstringstream replyString;
    replyString << L"201 TIME OK\r\n";
    replyString << ch->timecode().string(true);
    replyString << L"\r\n";
    return replyString.str();
}

std::wstring timecode_command(command_context& ctx)
{
    if (ctx.parameters.size() == 0) {
        std::wstringstream str;
        str << L"201 TIMECODE SOURCE OK\r\n";
        str << ctx.channel.raw_channel->stage()->timecode()->source_name();
        str << "\r\n";

        return str.str();
    }
    if (boost::iequals(ctx.parameters.at(0), L"CLOCK")) {
        ctx.channel.stage->execute([=]() { ctx.channel.raw_channel->stage()->timecode()->set_system_time(); });

        return L"202 TIMECODE SOURCE OK\r\n";
    }
    if (boost::iequals(ctx.parameters.at(0), L"LAYER")) {
        if (ctx.parameters.size() < 2) {
            return L"402 TIMECODE SOURCE FAILED\r\n";
        }

        const int layer = boost::lexical_cast<int>(ctx.parameters.at(1));
        ctx.channel.stage->execute([=]() {
            const auto producer = ctx.channel.stage->foreground(layer).share();
            ctx.channel.raw_channel->stage()->timecode()->set_weak_source(producer.get());
        });

        return L"202 TIMECODE SOURCE OK\r\n";
    }
    if (boost::iequals(ctx.parameters.at(0), L"CLEAR")) {
        ctx.channel.stage->execute([=]() { ctx.channel.raw_channel->stage()->timecode()->clear_source(); });

        return L"202 TIMECODE SOURCE OK\r\n";
    }

    return L"400 TIMECODE SOURCE FAILED\r\n";
}

void register_timecode_commands(std::shared_ptr<amcp_command_repository_wrapper>& repo)
{
    repo->register_channel_command(L"Timecode Commands", L"TIMECODE SOURCE", timecode_command, 0);

    repo->register_channel_command(L"Query Commands", L"TIME", time_command, 0);
}

}}} // namespace caspar::protocol::amcp