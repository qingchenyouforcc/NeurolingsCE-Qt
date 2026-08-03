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

#include "context.hpp"
#include "duktape/duktape.h"
#include <algorithm>
#include <stdexcept>
#include <shijima/behavior/base.hpp>

extern "C" int shijima_duk_exec_timeout_check(void *udata) {
    auto *deadline = static_cast<shijima::scripting::context::execution_deadline *>(udata);
    return deadline != nullptr && deadline->active &&
        std::chrono::steady_clock::now() >= deadline->expires;
}

namespace shijima {
namespace scripting {

math::vec2 context::duk_to_point(duk_context *ctx, duk_idx_t idx) {
    math::vec2 point;
    duk_get_prop_string(ctx, idx, "x");
    point.x = duk_to_number(ctx, -1);
    duk_pop(ctx);
    duk_get_prop_string(ctx, idx, "y");
    point.y = duk_to_number(ctx, -1);
    duk_pop(ctx);
    return point;
}

duk_ret_t context::duk_getter(duk_context *ctx) {
    // 0      1      2
    // [func] [this] [val]
    duk_push_current_function(ctx); // 0
    duk_push_this(ctx); // 1
    duk_get_prop_string(ctx, -2, DUK_HIDDEN_SYMBOL("prop")); // 2
    duk_get_prop(ctx, -2); // 2
    duk_copy(ctx, -1, -3); 
    duk_pop_2(ctx);
    return 1;
}

//FIXME: Setting objects like this will overwrite getters and setters
duk_ret_t context::duk_setter(duk_context *ctx) {
    // 0     1      2      3           4
    // [arg] [func] [this] [prop_name] [copy of arg]
    duk_push_current_function(ctx);
    duk_push_this(ctx);
    duk_get_prop_string(ctx, -2, DUK_HIDDEN_SYMBOL("prop"));
    duk_push_undefined(ctx);
    duk_copy(ctx, 0, duk_get_top_index(ctx));
    duk_put_prop(ctx, -3);
    duk_pop_2(ctx);
    return 1;
}

void context::put_prop_functions(std::string const& prop_name) {
    if (prop_name.size() <= 0) {
        throw std::logic_error("name.size() <= 0");
    }

    auto suffix = prop_name;
    suffix[0] = std::toupper(suffix[0]);

    // .getProp()
    auto getter = "get" + suffix;
    duk_push_c_function(duk, duk_getter, 0);
    duk_push_string(duk, prop_name.c_str());
    duk_put_prop_string(duk, -2, DUK_HIDDEN_SYMBOL("prop"));
    duk_put_prop_string(duk, -2, getter.c_str());

    // .setProp(value)
    auto setter = "set" + suffix;
    duk_push_c_function(duk, duk_setter, 1);
    duk_push_string(duk, prop_name.c_str());
    duk_put_prop_string(duk, -2, DUK_HIDDEN_SYMBOL("prop"));
    duk_put_prop_string(duk, -2, setter.c_str());
}

void context::put_prop(duk_idx_t obj, std::string const& prop_name) {
    if (prop_name.size() <= 0) {
        throw std::logic_error("name.size() <= 0");
    }
    duk_put_prop_string(duk, obj, prop_name.c_str());
    put_prop_functions(prop_name);
}

void context::duk_write_vararg_log(duk_context *duk, std::ostream &out) {
    duk_idx_t top = duk_get_top(duk);
    for (duk_idx_t i=0; i<top; i++) {
        out << duk_to_string(duk, i);
        if ((i + 1) != top) out << " ";
    }
    out << std::endl;
}

duk_ret_t context::duk_console_log(duk_context *duk) {
    duk_write_vararg_log(duk, std::cout);
    return 0;
}

duk_ret_t context::duk_console_error(duk_context *duk) {
    duk_write_vararg_log(duk, std::cerr);
    return 0;
}

duk_ret_t context::duk_function_callback(duk_context *duk) {
    duk_push_current_function(duk);
    duk_get_prop_string(duk, -1, DUK_HIDDEN_SYMBOL("stdfunc"));
    void *target_pt = duk_get_pointer(duk, -1);
    auto target = static_cast<std::function<duk_ret_t(duk_context *)>
        *>(target_pt);
    duk_pop(duk);
    return (*target)(duk);
}

duk_ret_t context::duk_finalizer_callback(duk_context *duk) {
    duk_push_current_function(duk);
    duk_get_prop_string(duk, -1, DUK_HIDDEN_SYMBOL("stdfunc"));
    void *target_pt = duk_get_pointer(duk, -1);
    auto target = static_cast<std::function<duk_ret_t(duk_context *,
        void *)> *>(target_pt);
    delete target;
    duk_pop(duk);
    return 0;
}

duk_idx_t context::push_function(std::function<duk_ret_t(duk_context *)> func,
    duk_idx_t nargs)
{
    auto heap_func = new std::function<duk_ret_t(duk_context *)>(func);
    duk_idx_t idx = duk_push_c_function(duk, duk_function_callback, nargs);
    duk_push_pointer(duk, static_cast<void *>(heap_func));
    duk_put_prop_string(duk, -2, DUK_HIDDEN_SYMBOL("stdfunc"));
    duk_push_c_function(duk, duk_finalizer_callback, 1);
    duk_set_finalizer(duk, -2);
    return idx;
}

void context::register_number_property(const char *name,
    std::function<double()> getter, std::function<void(double)> setter)
{
    duk_push_string(duk, name);
    duk_uint_t flags = DUK_DEFPROP_SET_ENUMERABLE;
    duk_idx_t idx = -2;
    if (getter != nullptr) {
        push_function([getter](duk_context *duk){
            duk_push_number(duk, getter());
            return (duk_ret_t)1;
        }, 0);
        idx -= 1;
        flags |= DUK_DEFPROP_HAVE_GETTER;
    }
    if (setter != nullptr) {
        push_function([setter](duk_context *duk){
            setter(duk_get_number(duk, 0));
            return (duk_ret_t)0;
        }, 1);
        idx -= 1;
        flags |= DUK_DEFPROP_HAVE_SETTER;
    }
    if (idx == -2) {
        throw std::invalid_argument("Both getter and setter are null.");
    }
    duk_def_prop(duk, idx, flags);
    put_prop_functions(name);
}

void context::register_boolean_property(const char *name,
    std::function<bool()> getter, std::function<void(bool)> setter)
{
    duk_push_string(duk, name);
    duk_uint_t flags = DUK_DEFPROP_SET_ENUMERABLE;
    duk_idx_t idx = -2;
    if (getter != nullptr) {
        push_function([getter](duk_context *duk){
            duk_push_boolean(duk, getter());
            return (duk_ret_t)1;
        }, 0);
        idx -= 1;
        flags |= DUK_DEFPROP_HAVE_GETTER;
    }
    if (setter != nullptr) {
        push_function([setter](duk_context *duk){
            setter(duk_get_boolean(duk, 0));
            return (duk_ret_t)0;
        }, 1);
        idx -= 1;
        flags |= DUK_DEFPROP_HAVE_SETTER;
    }
    if (idx == -2) {
        throw std::invalid_argument("Both getter and setter are null.");
    }
    duk_def_prop(duk, idx, flags);
    put_prop_functions(name);
}

void context::register_string_property(const char *name,
    std::function<bool(std::string &)> getter,
    std::function<void(std::string const&)> setter)
{
    duk_push_string(duk, name);
    duk_uint_t flags = DUK_DEFPROP_SET_ENUMERABLE;
    duk_idx_t idx = -2;
    if (getter != nullptr) {
        push_function([getter](duk_context *duk){
            std::string str;
            if (getter(str)) {
                duk_push_string(duk, str.c_str());
            }
            else {
                duk_push_null(duk);
            }
            return (duk_ret_t)1;
        }, 0);
        idx -= 1;
        flags |= DUK_DEFPROP_HAVE_GETTER;
    }
    if (setter != nullptr) {
        push_function([setter](duk_context *duk){
            const char *str = duk_get_string(duk, 0);
            if (str == NULL) {
                str = "";
            }
            setter(str);
            return (duk_ret_t)0;
        }, 1);
        idx -= 1;
        flags |= DUK_DEFPROP_HAVE_SETTER;
    }
    if (idx == -2) {
        throw std::invalid_argument("Both getter and setter are null.");
    }
    duk_def_prop(duk, idx, flags);
    put_prop_functions(name);
}

duk_idx_t context::build_console() {
    auto console = duk_push_bare_object(duk);
    duk_push_c_function(duk, duk_console_log, DUK_VARARGS);
    put_prop(-2, "log");
    duk_push_c_function(duk, duk_console_error, DUK_VARARGS);
    put_prop(-2, "error");
    duk_seal(duk, -1);
    return console;
}

duk_idx_t context::build_mascot() {
    auto mascot = duk_push_bare_object(duk);
    
    // mascot.bounds
    build_rectangle([this]() -> math::rec& { return this->state->bounds; });
    put_prop(-2, "bounds");

    // mascot.activeBehavior
    register_string_property("activeBehavior", [this](std::string &str) {
        if (!this->state->queued_behavior.empty()) {
            str = this->state->queued_behavior;
        }
        else if (this->state->behavior != nullptr) {
            str = this->state->behavior->name;
        }
        else {
            return false;
        }
        return true;
    }, [this](const std::string &str){
        this->state->queued_behavior = str;
    });

    // mascot.anchor
    build_vec2([this]() -> math::vec2& { return this->state->anchor; });
    put_prop(-2, "anchor");

    // mascot.lookRight
    register_boolean_property("lookRight",
        [this]() { return this->state->looking_right; },
        [this](bool value) { this->state->looking_right = value; });

    // mascot.totalCount
    register_number_property("totalCount",
        [this]() { return this->state->env->mascot_count; }, nullptr);
    
    // mascot.environment
    build_environment();
    put_prop(-2, "environment");

    duk_seal(duk, -1);    
    return mascot;
}
    
duk_idx_t context::build_area(std::function<mascot::environment::area&()> getter) {
    auto area = duk_push_bare_object(duk);

    // area.rightBorder
    build_border<mascot::environment::vborder>([getter]() { return getter().right_border(); });
    put_prop(-2, "rightBorder");

    // area.leftBorder
    build_border<mascot::environment::vborder>([getter]() { return getter().left_border(); });
    put_prop(-2, "leftBorder");

    // area.topBorder
    build_border<mascot::environment::hborder>([getter]() { return getter().top_border(); });
    put_prop(-2, "topBorder");

    // area.bottomBorder
    build_border<mascot::environment::hborder>([getter]() { return getter().bottom_border(); });
    put_prop(-2, "bottomBorder");

    // area.width
    register_number_property("width", [getter]() { return getter().width(); }, \
        nullptr);

    // area.height
    register_number_property("height", [getter]() { return getter().height(); }, \
        nullptr);
    
    // area.visible
    register_boolean_property("visible", [getter]() { return getter().visible(); },
        nullptr);

    // area.left, area.right, area.top, area.bottom
    #define reg(side) \
        register_number_property(#side, [getter]() { return getter().side; }, \
        [getter](double val) { getter().side = val; })
    reg(left); reg(right); reg(top); reg(bottom);
    #undef reg

    duk_seal(duk, -1);
    return area;
}

duk_idx_t context::build_proxy() {
    duk_push_global_object(duk);

    // proxy._activeGlobal
    duk_push_literal(duk, "_activeGlobal");
    push_function([this](duk_context *duk){
        if (global_stack.size() == 0) {
            duk_push_this(duk);
            return 1;
        }
        auto active = global_stack[global_stack.size()-1];
        duk_push_this(duk); // -3
        duk_get_prop_literal(duk, -1, DUK_HIDDEN_SYMBOL("_globals")); // -2
        duk_get_prop_index(duk, -1, active); // -1
        duk_copy(duk, -1, duk_normalize_index(duk, -3));
        duk_pop_2(duk);
        return 1;
    }, 0);
    duk_def_prop(duk, -3, DUK_DEFPROP_HAVE_GETTER);

    // proxy._globalCount
    duk_push_literal(duk, "_globalCount");
    push_function([this](duk_context *duk){
        duk_push_number(duk, this->globals_available);
        return 1;
    }, 0);
    duk_def_prop(duk, -3, DUK_DEFPROP_HAVE_GETTER);

    // proxy._hasConstant(name)
    duk_push_literal(duk, "_hasConstant");
    push_function([this](duk_context *duk){
        std::string name = duk_get_string(duk, 0);
        duk_push_boolean(duk, this->state->constants.count(name) == 1);
        return 1;
    }, 1);
    duk_put_prop(duk, -3);

    // proxy._getConstant(name)
    duk_push_literal(duk, "_getConstant");
    push_function([this](duk_context *duk){
        std::string name = duk_get_string(duk, 0);
        if (this->state->constants.count(name) == 0) {
            duk_push_undefined(duk);
            return 1;
        }
        //FIXME: This may not be right
        auto value = this->state->constants.at(name);
        duk_eval_string(duk, value.c_str());
        return 1;
    }, 1);
    duk_put_prop(duk, -3);

    // proxy[HiddenSymbol("_globals")]
    duk_push_literal(duk, DUK_HIDDEN_SYMBOL("_globals"));
    duk_push_bare_object(duk);
    duk_def_prop(duk, -3, DUK_DEFPROP_HAVE_VALUE);
    duk_pop(duk);
    const char *builder =
        "(function(target){"
        "    return new Proxy(target, {"
        "        defineProperty(target, property, descriptor) {"
        "            if (property === \"_activeGlobal\" || property ==="
        "                \"_getConstant\" || property === \"_hasConstant\") return;"
        "            const real = target._activeGlobal;"
        "            return Reflect.defineProperty(real, property, descriptor);"
        "        },"
        "        \"set\": function(target, property, value, receiver) {"
        "            if (property === \"_activeGlobal\" || property ==="
        "                \"_getConstant\" || property === \"_hasConstant\") return;"
        "            const real = target._activeGlobal;"
        "            return Reflect.set(real, property, value);"
        "        },"
        "        deleteProperty(target, property, descriptor) {"
        "            if (property === \"_activeGlobal\" || property ==="
        "                \"_getConstant\" || property === \"_hasConstant\") return;"
        "            const real = target._activeGlobal;"
        "            return Reflect.deleteProperty(real, property, descriptor);"
        "        },"
        "        \"get\": function(target, property, receiver) {"
        "            const mascotConstant = target._getConstant(property);"
        "            if (mascotConstant != null) {"
        "                return mascotConstant;"
        "            }"
        "            const real = target._activeGlobal;"
        "            if (property in real) {"
        "                return Reflect.get(real, property);"
        "            }"
        "            else {"
        "                return Reflect.get(target, property);"
        "            }"
        "        },"
        "        has(target, property) {"
        "            const real = target._activeGlobal;"
        "            return target._hasConstant(property) || (property in real) ||"
        "                (property in target);"
        "        }"
        "    })"
        "})(globalThis)";
    duk_eval_string(duk, builder);
    duk_seal(duk, -1);
    return duk_get_top_index(duk);
}
    
duk_idx_t context::build_environment() {
    auto env = duk_push_bare_object(duk);

    // environment.floor
    build_border<mascot::environment::hborder>([this]() { return this->state->env->floor; });
    put_prop(-2, "floor");

    // environment.ceiling
    build_border<mascot::environment::hborder>([this]() { return this->state->env->ceiling; });
    put_prop(-2, "ceiling");

    // environment.wall
    auto wall = duk_push_bare_object(duk);
    push_function([this](duk_context *ctx) -> duk_ret_t {
        auto point = duk_to_point(ctx, 0);
        auto &env = *this->state->env;
        bool isOn = env.work_area.left_border().is_on(point) ||
            env.work_area.right_border().is_on(point) ||
            env.active_ie.left_border().is_on(point) ||
            env.active_ie.right_border().is_on(point);
        duk_push_boolean(ctx, isOn);
        return 1;
    }, 1);
    duk_put_prop_string(duk, -2, "isOn");
    push_function([this](duk_context *ctx) -> duk_ret_t {
        auto point = duk_to_point(ctx, 0);
        auto &env = *this->state->env;
        bool faces = env.work_area.left_border().faces(point) ||
            env.work_area.right_border().faces(point) ||
            env.active_ie.left_border().faces(point) ||
            env.active_ie.right_border().faces(point);
        duk_push_boolean(ctx, faces);
        return 1;
    }, 1);
    duk_put_prop_string(duk, -2, "faces");
    duk_seal(duk, -1);
    put_prop(-2, "wall");

    // environment.workArea
    build_area([this]() -> mascot::environment::area& { return this->state->env->work_area; });
    put_prop(-2, "workArea");

    // environment.screen
    build_area([this]() -> mascot::environment::area& { return this->state->env->screen; });
    put_prop(-2, "screen");

    // environment.activeIE
    build_area([this]() -> mascot::environment::area& { return this->state->env->active_ie; });
    put_prop(-2, "activeIE");

    // Pushing an external window is a user-controlled capability. Expose it
    // to behavior conditions so disabled pushes are not selected.
    register_boolean_property("allowsWindowPushing",
        [this]() { return this->state->env->allows_window_pushing; }, nullptr);

    // environment.cursor
    build_dvec2([this]() -> mascot::environment::dvec2 { return this->state->get_cursor(); });
    put_prop(-2, "cursor");

    duk_seal(duk, -1);
    return env;
}

duk_idx_t context::build_rectangle(std::function<math::rec&()> getter) {
    auto rect = duk_push_bare_object(duk);
    #define reg(x) \
        register_number_property(#x, [getter]() { return getter().x; }, \
        [getter](double val) { getter().x = val; })
    reg(x); reg(y); reg(width); reg(height);
    #undef reg
    duk_seal(duk, -1);
    return rect;
}

duk_idx_t context::build_vec2(std::function<math::vec2&()> getter) {
    auto vec2 = duk_push_bare_object(duk);
    #define reg(x) \
        register_number_property(#x, [getter]() { return getter().x; }, \
        [getter](double val) { getter().x = val; })
    reg(x); reg(y);
    #undef reg
    duk_seal(duk, -1);
    return vec2;
}

duk_idx_t context::build_dvec2(std::function<mascot::environment::dvec2()> getter) {
    auto vec2 = duk_push_bare_object(duk);
    #define reg(x) \
        register_number_property(#x, [getter]() { return getter().x; }, \
        nullptr)
    reg(x); reg(y); reg(dx); reg(dy);
    #undef reg
    duk_seal(duk, -1);
    return vec2;
}

bool context::has_global(duk_uarridx_t idx) {
    duk_push_global_object(duk);
    duk_get_prop_string(duk, -1, DUK_HIDDEN_SYMBOL("_globals"));
    duk_get_prop_index(duk, -1, idx);
    auto type = duk_get_type(duk, -1);
    duk_pop_3(duk);
    return type != DUK_TYPE_UNDEFINED;
}

bool context::is_global_active(duk_uarridx_t idx) {
    return std::find(global_stack.begin(), global_stack.end(),
        idx) != global_stack.end();
}

void context::create_global(duk_uarridx_t idx) {
    if (has_global(idx)) {
        throw std::logic_error("global index already in use");
    }
    duk_push_global_object(duk);
    duk_get_prop_string(duk, -1, DUK_HIDDEN_SYMBOL("_globals"));
    duk_push_bare_object(duk);
    duk_put_prop_index(duk, -2, idx);
    duk_pop_2(duk);
    globals_available++;
}

void context::delete_global(duk_uarridx_t idx) {
    if (!has_global(idx)) {
        throw std::logic_error("cannot delete non-existing global");
    }
    if (is_global_active(idx)) {
        throw std::logic_error("cannot delete active global");
    }
    duk_push_global_object(duk);
    duk_get_prop_string(duk, -1, DUK_HIDDEN_SYMBOL("_globals"));
    duk_del_prop_index(duk, -1, idx);
    duk_pop_2(duk);
    globals_available--;
}

void context::push_global(duk_uarridx_t idx) {
    if (!has_global(idx)) {
        throw std::logic_error("no such global");
    }
    global_stack.push_back(idx);
}

void context::pop_global(duk_uarridx_t idx) {
    if (global_stack.empty() || global_stack[global_stack.size()-1] != idx) {
        throw std::logic_error("idx != global_stack.top()");
    }
    global_stack.erase(global_stack.begin() + global_stack.size() - 1);
}

duk_uarridx_t context::next_global_idx() {
    while (has_global(++global_counter));
    auto idx = global_counter;
    return idx;
}

void context::global::check_valid() {
    if (invalidated != nullptr && *invalidated) {
        ctx = nullptr;
        if (is_active) {
            throw std::logic_error("global was invalidated while active");
        }
        is_active = false;
        invalidated = nullptr;
    }
}
void context::global::init() {
    check_valid();
    if (is_active) {
        throw std::logic_error("init() called on active global");
    }
    is_active = true;
    ctx->push_global(idx);
}
void context::global::finalize() {
    check_valid();
    if (!is_active) {
        throw std::logic_error("finalize() called on inactive global");
    }
    is_active = false;
    ctx->pop_global(idx);
}
context::global::global(context *ctx, duk_uarridx_t idx,
    std::shared_ptr<bool> invalidated): idx(idx), ctx(ctx),
    is_active(false), invalidated(invalidated) {}

context::global::active::active(global *owner): owner(owner) {}

context *context::global::active::get() {
    return owner->ctx;
}

context *context::global::active::operator->() {
    return owner->ctx;
}

context::global::active::active(context::global::active &&rhs) {
    owner = rhs.owner;
    rhs.owner = nullptr;
}

context::global::active &context::global::active::operator=(context::global::active &&rhs) {
    if (owner != nullptr) {
        owner->finalize();
    }
    owner = rhs.owner;
    rhs.owner = nullptr;
    return *this;
}

context::global::active::~active() {
    if (owner != nullptr) {
        owner->finalize();
    }
}

#define move() do { \
    check_valid(); \
    if (is_active || rhs.is_active) { \
        throw std::logic_error("cannot destroy active global"); \
    } \
    if (ctx != nullptr) { \
        ctx->delete_global(idx); \
    } \
    invalidated = rhs.invalidated; \
    ctx = rhs.ctx; \
    idx = rhs.idx; \
    rhs.ctx = nullptr; \
    rhs.invalidated = nullptr; \
} while(0)

context::global::global(context::global &&rhs): ctx(nullptr), is_active(false) {
    move();
}
context::global &context::global::operator=(context::global &&rhs) {
    move();
    return *this;
}
#undef move

context::global::~global() {
    check_valid();
    if (ctx != nullptr) {
        if (is_active) {
            finalize();
        }
        ctx->delete_global(idx);
    }
}

context::global::active context::global::use() {
    init();
    return { this };
}

bool context::global::valid() {
    return ctx != nullptr;
}

context::global::global(): idx(0), ctx(nullptr), is_active(false) {}

context::global context::make_global() {
    auto idx = next_global_idx();
    create_global(idx);
    return { this, idx, invalidated_flag };
}

double context::random() {
    return state->env->random();
}

int context::random(int upper_range) {
    return state->env->random(upper_range);
}

bool context::eval_bool(std::string const& js, bool log) {
    bool ret;
    try {
        duk_eval_string(duk, js.c_str());
        ret = duk_to_boolean(duk, -1);
    }
    catch (std::exception &ex) {
        #ifdef SHIJIMA_LOGGING_ENABLED
            shijima::log(SHIJIMA_LOG_WARNINGS, "warning: eval_bool() failed: "
                + std::string(ex.what()));
            shijima::log(SHIJIMA_LOG_WARNINGS, "warning: relevant script: "
                + normalize_js(js));
        #endif
        ret = false;
    }
    if (log) {
        log_javascript(js, (ret ? "true" : "false"));
    }
    duk_pop(duk);
    return ret;
}

bool context::eval_bool_with_timeout(std::string const& js,
    std::chrono::milliseconds timeout, bool log)
{
    deadline.expires = std::chrono::steady_clock::now() + timeout;
    deadline.active = true;
    bool result = false;
    if (duk_peval_string(duk, js.c_str()) == DUK_EXEC_SUCCESS) {
        result = duk_to_boolean(duk, -1);
        if (log) {
            log_javascript(js, result ? "true" : "false");
        }
    }
    deadline.active = false;
    duk_pop(duk);
    return result;
}

double context::eval_number(std::string js, bool log) {
    duk_eval_string(duk, js.c_str());
    double ret = duk_to_number(duk, -1);
    if (log) {
        log_javascript(js, std::to_string(ret));
    }
    duk_pop(duk);
    return ret;
}

std::string context::eval_string(std::string js, bool log) {
    duk_eval_string(duk, js.c_str());
    std::string ret = duk_to_string(duk, -1);
    if (log) {
        log_javascript(js, ret);
    }
    duk_pop(duk);
    return ret;
}

std::string context::normalize_js(std::string js) {
    size_t i;
    for (i=0; (i = js.find_first_of("\r\t\n", i)) != std::string::npos;) {
        js[i] = ' ';
    }
    return js;
}


void context::log_javascript(std::string const& js, std::string const& result) {
    #ifdef SHIJIMA_LOGGING_ENABLED
        if (get_log_level() & SHIJIMA_LOG_JAVASCRIPT) {
            log("\"" + normalize_js(js) + "\" = " + result);
        }
    #else
        (void)js; (void)result;
    #endif
}

std::string context::eval_json(std::string js) {
    duk_get_global_string(duk, "JSON");
    duk_get_prop_string(duk, -1, "stringify");
    duk_remove(duk, -2);
    duk_eval_string(duk, js.c_str());
    duk_call(duk, 1);
    std::string ret = duk_to_string(duk, -1);
    duk_pop(duk);
    return ret;
}

void context::eval(std::string js) {
    duk_eval_string_noresult(duk, js.c_str());
}

context::context() {
    invalidated_flag = std::make_shared<bool>(false);
    duk = duk_create_heap(nullptr, nullptr, nullptr, &deadline, nullptr);
    build_mascot();
    duk_put_global_string(duk, "mascot");
    build_console();
    duk_put_global_string(duk, "console");

    // Math.random()
    duk_get_global_string(duk, "Math");
    std::function<duk_ret_t (duk_context *)> random_func = [this](duk_context *duk) {
        duk_push_number(duk, this->state->env->random());
        return 1;
    };
    push_function(random_func, 0);
    push_function(random_func, 0);
    // (Math.random * x) = (Math.random.valueOf() * x) = (Math.random() * x)
    // Required by some shimeji
    duk_put_prop_string(duk, -2, "valueOf");
    duk_put_prop_string(duk, -2, "random");
    duk_pop(duk);

    // Proxy global
    build_proxy();
    duk_set_global_object(duk);

    // initial global to prevent the real global
    // from being modified
    auto idx = next_global_idx();
    create_global(idx);
    push_global(idx);
}

context::~context() {
    *invalidated_flag = true;
    duk_destroy_heap(duk);
}

}
}
