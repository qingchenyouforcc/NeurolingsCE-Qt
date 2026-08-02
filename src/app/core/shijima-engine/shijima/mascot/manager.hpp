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

#include "state.hpp"
#include "tick.hpp"
#include <shijima/behavior/manager.hpp>
#include <shijima/scripting/context.hpp>
#include <shijima/parser.hpp>
#include <shijima/log.hpp>
#include <memory>

namespace shijima {
namespace mascot {

class manager {
private:
    behavior::manager behaviors;
    mascot::tick tick_ctx;
    std::shared_ptr<action::base> action;
    std::map<std::string, std::string> constants;
public:
    behavior::list const& initial_behavior_list() {
        return behaviors.get_initial_list();
    }
private:
    void _next_behavior(std::string const& name = "");
    bool action_tick();
    bool has_queued_behavior();
    void activate_queued_behavior();
public:
    void reset_position();
    void detach_from_borders();
    void next_behavior(std::string const& name = "");
    void prefer_next_behavior(std::string const& name);
    void clear_preferred_next_behavior();
    std::string hotspot_behavior(math::vec2 cursor);
    bool trigger_hotspot(math::vec2 cursor);

    struct initializer {
        math::vec2 anchor;
        std::string behavior;
        bool looking_right;
        initializer(math::vec2 anchor = {0,0}, std::string behavior = "",
            bool looking_right = false):
            anchor(anchor), behavior(behavior), looking_right(looking_right) {}
        initializer(mascot::state::breed_request_data const& data):
            anchor(data.anchor), behavior(data.behavior),
            looking_right(data.looking_right) {}
    };

    std::shared_ptr<scripting::context> script_ctx;
    std::shared_ptr<mascot::state> state;

    manager &operator=(manager const&) = delete;
    manager &operator=(manager&&) = default;
    manager(manager const&) = delete;
    manager(manager&&) = default;

    std::shared_ptr<const behavior::base> active_behavior();
    manager(std::string const& actions_xml, std::string const& behaviors_xml,
        initializer init = {}, std::shared_ptr<scripting::context> script_ctx = nullptr);
    std::string export_state();
private:
    void pre_tick();
    void post_tick();
    bool _tick();
public:
    void tick();
};

}
}
