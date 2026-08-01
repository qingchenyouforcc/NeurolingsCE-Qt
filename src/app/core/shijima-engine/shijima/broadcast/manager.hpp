#pragma once

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

#include "client.hpp"
#include "server.hpp"
#include <string>
#include <map>
#include <vector>

namespace shijima {
namespace broadcast {

class manager {
private:
    std::map<std::string, std::vector<server>> m_servers;
public:
    server start_broadcast(std::string const& affordance, math::vec2 anchor);
    bool try_connect(client &peer, math::vec2 anchor, std::string const& affordance,
        std::string const& client_behavior, std::string const& server_behavior);
};

}
}
