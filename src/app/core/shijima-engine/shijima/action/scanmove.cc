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

#include "scanmove.hpp"
#include <cmath>
#include <shijima/log.hpp>

namespace shijima {
namespace action {

bool scanmove::requests_broadcast() {
    return false;
}

void scanmove::init(mascot::tick &ctx) {
    move::init(ctx);
    if (mascot->env == nullptr || mascot->env->broadcasts == nullptr) {
        return;
    }
    mascot->env->broadcasts->try_connect(client, mascot->anchor,
        vars.get_string("Affordance"), vars.get_string("Behavior"),
        vars.get_string("TargetBehavior"));
}

bool scanmove::tick() {
    if (!client.connected()) {
        return false;
    }
    vars.add_attr({ { "TargetX", client.get_target().x } });
    bool ret = move::tick();
    auto target = client.get_target();
    if (std::fabs(mascot->anchor.x - target.x) < 3) {
        mascot->anchor.x = target.x;
        client.notify_arrival();
        mascot->interaction = client.get_interaction();
        mascot->queued_behavior = mascot->interaction.behavior();
        #ifdef SHIJIMA_LOGGING_ENABLED
            log(SHIJIMA_LOG_BROADCASTS, "Client did meet server, starting interaction");
            log(SHIJIMA_LOG_BROADCASTS, "Queued behavior: " + mascot->queued_behavior);
        #endif
        return true;
    }
    return ret;
}

}
}
