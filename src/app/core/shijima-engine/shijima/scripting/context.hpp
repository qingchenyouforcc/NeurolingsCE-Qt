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

#include "duktape/duk_config.h"
#include <shijima/math.hpp>
#include <shijima/mascot/state.hpp>
#include <shijima/log.hpp>
#include <functional>
#include <vector>
#include <iostream>
#include <memory>
#include <chrono>

// forward declarations from duktape.h
extern "C" {
    DUK_EXTERNAL_DECL duk_idx_t duk_push_bare_object(duk_context *ctx);
    DUK_EXTERNAL_DECL duk_bool_t duk_put_prop_string(duk_context *ctx, duk_idx_t obj_idx, const char *key);
    DUK_EXTERNAL_DECL void duk_push_boolean(duk_context *ctx, duk_bool_t val);
}

namespace shijima {
namespace scripting {

class context {
private:
    static math::vec2 duk_to_point(duk_context *ctx, duk_idx_t idx);
    static duk_ret_t duk_getter(duk_context *ctx);
    static duk_ret_t duk_setter(duk_context *ctx);
    static std::string normalize_js(std::string js);

    duk_idx_t build_mascot();
    duk_idx_t build_console();
    duk_idx_t build_area(std::function<mascot::environment::area&()> getter);
    void put_prop(duk_idx_t obj, std::string const& name);
    void put_prop_functions(std::string const& prop_name);
    void log_javascript(std::string const& js, std::string const& result);

    template<typename T>
    duk_idx_t build_border(std::function<T()> getter) {
        auto border = duk_push_bare_object(duk);

        // leftBorder.isOn(point)
        //   where point = { x: number, y: number }
        push_function([getter](duk_context *ctx) -> duk_ret_t {
            auto point = duk_to_point(ctx, 0);
            duk_push_boolean(ctx, getter().is_on(point));
            return 1;
        }, 1);
        duk_put_prop_string(duk, -2, "isOn");

        return border;
    }
    
    duk_idx_t build_environment();
    duk_idx_t build_rectangle(std::function<math::rec&()> getter);
    duk_idx_t build_vec2(std::function<math::vec2&()> getter);
    duk_idx_t build_dvec2(std::function<mascot::environment::dvec2()> getter);
    static void duk_write_vararg_log(duk_context *ctx, std::ostream &out);
    static duk_ret_t duk_console_log(duk_context *ctx);
    static duk_ret_t duk_console_error(duk_context *ctx);
    static duk_ret_t duk_function_callback(duk_context *ctx);
    static duk_ret_t duk_finalizer_callback(duk_context *ctx);
    duk_idx_t build_proxy();
    duk_idx_t push_function(std::function<duk_ret_t(duk_context *)> func,
        duk_idx_t nargs);
    void register_number_property(const char *name,
        std::function<double()> getter, std::function<void(double)> setter);
    void register_boolean_property(const char *name,
        std::function<bool()> getter, std::function<void(bool)> setter);
    void register_string_property(const char *name,
        std::function<bool(std::string &)> getter,
        std::function<void(std::string const&)> setter);

    duk_uarridx_t global_counter = 0;
    duk_uarridx_t globals_available = 0;
    std::vector<duk_uarridx_t> global_stack;
    bool has_global(duk_uarridx_t idx);
    bool is_global_active(duk_uarridx_t idx);
    void create_global(duk_uarridx_t idx);
    void delete_global(duk_uarridx_t idx);
    void push_global(duk_uarridx_t idx);
    void pop_global(duk_uarridx_t idx);
    duk_uarridx_t next_global_idx();

    std::shared_ptr<bool> invalidated_flag;
public:
    struct execution_deadline {
        bool active = false;
        std::chrono::steady_clock::time_point expires;
    };
    // Holds a bare pointer to the context. The context should therefore
    // outlive any global. In case it doesn't, the invalidated flag
    // will prevent bad memory access.
    class global {
    private:
        friend class context;
        duk_uarridx_t idx;
        context *ctx;
        bool is_active;
        std::shared_ptr<bool> invalidated;
        void check_valid();
        void init();
        void finalize();
        global(context *ctx, duk_uarridx_t idx,
            std::shared_ptr<bool> invalidated);
    public:
        // Holds a bare pointer to the global. Intended to be used on the stack.
        // Must outlive the global. Will cause a crash if the global is freed
        // first.
        class active {
        private:
            friend class global;
            global *owner;
            active(global *owner);
        public:
            context *get();
            context *operator->();
            active(active const&) = delete;
            active& operator=(active const&) = delete;
            active(active &&rhs);
            active &operator=(active &&rhs);
            ~active();
        };
        global(global const&) = delete;
        global& operator=(global const&) = delete;
        global(global &&rhs);
        global &operator=(global &&rhs);
        ~global();
        active use();
        bool valid();
        global();
    };
    duk_context *duk;
    std::shared_ptr<mascot::state> state;
    global make_global();
    double random();
    int random(int upper_range);
#ifdef SHIJIMA_LOGGING_ENABLED
#define log_javascript_default true
#else
#define log_javascript_default false
#endif
    bool eval_bool(std::string const& js, bool log = log_javascript_default);
    bool eval_bool_with_timeout(std::string const& js,
        std::chrono::milliseconds timeout, bool log = false);
    double eval_number(std::string js, bool log = log_javascript_default);
    std::string eval_string(std::string js, bool log = log_javascript_default);
#undef log_javascript
    std::string eval_json(std::string js);
    void eval(std::string js);
    context();
    ~context();

    context &operator=(context const&) = delete;
    context &operator=(context&&) = delete;
    context(context const&) = delete;
    context(context&&) = delete;
private:
    execution_deadline deadline;
};

}
}
