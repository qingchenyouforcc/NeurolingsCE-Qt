// 
// libshijima - C++ library for shimeji desktop mascots
// Copyright (C) 2024-2025 pixelomer
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
// 

#include "manager.hpp"
#include <algorithm>
#include <cmath>
#include <shijima/log.hpp>

namespace shijima {
namespace broadcast {

//FIXME: This implementation does not scale well

namespace {

// ScanMove interactions should only claim a mascot that is already nearby.
// Shimeji artwork conventionally uses a 128px canvas, so two canvas widths
// leave enough room for a short approach without targeting across the desktop.
constexpr double max_scan_distance = 256.0;

}

server manager::start_broadcast(std::string const& affordance, math::vec2 anchor) {
    #ifdef SHIJIMA_LOGGING_ENABLED
        log(SHIJIMA_LOG_BROADCASTS, "Broadcasting affordance: " + affordance);
    #endif
    server new_server { anchor };
    m_servers[affordance].push_back(new_server);
    return new_server;
}

bool manager::try_connect(client &peer, math::vec2 anchor,
    std::string const& affordance,
    std::string const& client_behavior, std::string const& server_behavior)
{
    #ifdef SHIJIMA_LOGGING_ENABLED
        log(SHIJIMA_LOG_BROADCASTS, "Trying to connect: " + affordance);
    #endif
    auto &servers = m_servers[affordance];
    servers.erase(std::remove_if(servers.begin(), servers.end(),
        [](server &candidate) { return !candidate.active(); }), servers.end());

    server *nearest = nullptr;
    double nearest_distance = max_scan_distance;
    for (auto &candidate : servers) {
        auto target = candidate.get_anchor();
        double distance = std::fabs(anchor.x - target.x);
        if (std::fabs(anchor.y - target.y) > 1 ||
            distance > max_scan_distance || !candidate.available())
        {
            continue;
        }
        if (nearest == nullptr || distance < nearest_distance) {
            nearest = &candidate;
            nearest_distance = distance;
        }
    }
    if (nearest == nullptr) {
        return false;
    }

    peer = nearest->connect(client_behavior, server_behavior);
    #ifdef SHIJIMA_LOGGING_ENABLED
        log(SHIJIMA_LOG_BROADCASTS, "Connected: " + affordance);
    #endif
    return true;
}

}
}
