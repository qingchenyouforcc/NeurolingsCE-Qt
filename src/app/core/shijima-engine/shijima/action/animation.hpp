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

#include "base.hpp"
#include <shijima/animation.hpp>

namespace shijima {
namespace action {

class animation : public base {
private:
    bool has_fixed_velocity;
    math::vec2 fixed_velocity;
    int current_anim_time;
    std::shared_ptr<shijima::animation> current_anim;
protected:
    int anim_idx;
    virtual std::shared_ptr<shijima::animation> &get_animation();
    pose const& get_pose();
    math::vec2 get_velocity();
    bool animation_finished();
    virtual bool check_border_type();
    virtual bool handle_dragging();
public:
    virtual bool requests_broadcast() override;
    std::vector<std::shared_ptr<shijima::animation>> animations;
    std::string hotspot_behavior_at(math::vec2 cursor);
    virtual void init(mascot::tick &ctx) override;
    virtual bool tick() override;
    virtual void finalize() override;
protected:
    bool window_push_requested = false;
    bool is_window_push_action() const;
};

}
}
