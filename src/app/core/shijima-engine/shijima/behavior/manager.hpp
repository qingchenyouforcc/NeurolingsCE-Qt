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

#include <memory>
#include "base.hpp"
#include "list.hpp"
#include <shijima/scripting/context.hpp>
#include <shijima/mascot/state.hpp>

namespace shijima {
namespace behavior {

class manager {
public:
    list const& get_initial_list();
private:
    list initial_list;
    list next_list;
    scripting::context::global global;
public:
    manager &operator=(manager const&) = delete;
    manager &operator=(manager&&) = default;
    manager(manager const&) = delete;
    manager(manager&&) = default;
    manager();
    manager(scripting::context &ctx, list initial_list,
        std::string const& first_behavior);
    void set_next(std::string const& next_name);
    // Restore the next-list selected by a currently active behavior after a
    // temporary set_next() preference has been cleared.
    void restore_next(std::shared_ptr<base> behavior);
    std::shared_ptr<base> next(std::shared_ptr<mascot::state> state);
};

}
}
