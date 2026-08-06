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

#include "fall.hpp"

namespace shijima {
namespace action {

void fall::init(mascot::tick &ctx) {
    animation::init(ctx);
    velocity.x = vars.get_num("InitialVX", 0);
    velocity.y = vars.get_num("InitialVY", 0);
}

bool fall::requests_interpolation() {
    return false;
}

bool fall::subtick(int idx) {
    if (!animation::subtick(idx)) {
        return false;
    }
    bool on_land = mascot->env->floor.is_on(mascot->anchor) ||
        mascot->env->ceiling.is_on(mascot->anchor) ||
        mascot->env->work_area.is_on(mascot->anchor);
    if (elapsed() > 0) {
        // Don't consider IE on the first tick
        on_land = on_land ||
            mascot->env->active_ie.is_on(mascot->anchor);
    }
    if (on_land) {
        return false;
    }

    if (velocity.x != 0) {
        mascot->looking_right = (velocity.x > 0);
    }

    auto subtick_count = mascot->env->sanitized_subtick_count();

    math::vec2 resistance;
    resistance.x = vars.get_num("RegistanceX", 0.05);
    resistance.y = vars.get_num("RegistanceY", 0.1);
    double gravity = vars.get_num("Gravity", 2);
    velocity.x -= (velocity.x * resistance.x) / subtick_count;
    velocity.y += (gravity - velocity.y * resistance.y) / subtick_count;
    
    vars.add_attr({{ "VelocityX", velocity.x }, { "VelocityY", velocity.y }});

    math::vec2 before = mascot->anchor;

    mascot->anchor.x += velocity.x / subtick_count;
    mascot->anchor.y += velocity.y / subtick_count;
    
    if (mascot->anchor.x > mascot->env->work_area.right) {
        mascot->anchor.x = mascot->env->work_area.right;
    }
    else if (mascot->anchor.x < mascot->env->work_area.left) {
        mascot->anchor.x = mascot->env->work_area.left;
    }
    if (mascot->anchor.y < mascot->env->ceiling.y) {
        mascot->anchor.y = mascot->env->ceiling.y;
    }
    else if (mascot->anchor.y > mascot->env->floor.y) {
        mascot->anchor.y = mascot->env->floor.y;
    }

    math::vec2 after = mascot->anchor;

    #define AREA_STICK(area, axis, side, before_comp, after_comp) \
            (mascot->env->area.visible() && \
            mascot->env->area.side##_border().faces(after) && \
            before.axis before_comp mascot->env->area.side && \
            after.axis after_comp mascot->env->area.side) \
        do { mascot->anchor.axis = mascot->env->area.side; } \
        while (0)

    #define IE_STICK(axis, side, before_comp, after_comp) \
        AREA_STICK(active_ie, axis, side, before_comp, after_comp)
    
    if      IE_STICK(x, left, <=, >=);
    else if IE_STICK(x, right, >=, <=);
    else if IE_STICK(y, top, <=, >=);
    else if IE_STICK(y, bottom, >=, <=);

    #undef IE_STICK
    #undef AREA_STICK

    // Global boundaries take priority over the active-window interaction box
    // when they conflict on the same axis. Win32 window rects use an
    // exclusive bottom/right edge while Qt screen geometry is inclusive, so
    // a maximized window next to the taskbar can expose active_ie.bottom one
    // pixel past floor.y. Without this final clamp the IE stick would push
    // the anchor below the floor and re-enter Fall forever.
    if (mascot->anchor.x > mascot->env->work_area.right) {
        mascot->anchor.x = mascot->env->work_area.right;
    }
    else if (mascot->anchor.x < mascot->env->work_area.left) {
        mascot->anchor.x = mascot->env->work_area.left;
    }
    if (mascot->anchor.y < mascot->env->ceiling.y) {
        mascot->anchor.y = mascot->env->ceiling.y;
    }
    else if (mascot->anchor.y > mascot->env->floor.y) {
        mascot->anchor.y = mascot->env->floor.y;
    }

    return true;
}

}
}
