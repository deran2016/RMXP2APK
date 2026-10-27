#include "graphics.h"
#include "bitmap.h"
#include <ruby.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <thread>

namespace {
LunetileSurface* output = nullptr;
bool (*wait_handler)() = nullptr;
std::unique_ptr<lunetile::Raster> frame, group, frozen, transitioned;
const lunetile::Raster* last_output = nullptr;
long frame_count = 0;
int frame_rate = 40;
bool stock120 = false;
std::chrono::steady_clock::time_point deadline;
struct Clip { int x, y, w, h; };
VALUE error_class() { return rb_const_get(rb_cObject, rb_intern("RGSSError")); }
int number(VALUE value) { return NUM2INT(value); }
double finite(VALUE value) {
    double n = NUM2DBL(value);
    if (!std::isfinite(n)) rb_raise(error_class(), "non-finite render value");
    return n;
}
int bounded(double value) {
    if (!std::isfinite(value) || value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
        rb_raise(error_class(), "render coordinate out of range");
    return int(std::round(value));
}
double property(VALUE value, const char* name) { return finite(rb_funcall(value, rb_intern(name), 0)); }
Clip rectangle(VALUE rect) {
    return {number(rb_funcall(rect, rb_intern("x"), 0)), number(rb_funcall(rect, rb_intern("y"), 0)),
            number(rb_funcall(rect, rb_intern("width"), 0)), number(rb_funcall(rect, rb_intern("height"), 0))};
}
Clip intersect(Clip a, Clip b) {
    int64_t x = std::max<int64_t>(a.x, b.x), y = std::max<int64_t>(a.y, b.y);
    int64_t right = std::min<int64_t>(int64_t(a.x) + std::max(a.w, 0), int64_t(b.x) + std::max(b.w, 0));
    int64_t bottom = std::min<int64_t>(int64_t(a.y) + std::max(a.h, 0), int64_t(b.y) + std::max(b.h, 0));
    if (right <= x || bottom <= y) return {0, 0, 0, 0};
    return {int(x), int(y), int(right - x), int(bottom - y)};
}
VALUE present(VALUE, VALUE commands, VALUE transition, VALUE mask, VALUE vague) {
    Check_Type(commands, T_ARRAY);
    if (RARRAY_LEN(commands) > 100000) rb_raise(rb_eArgError, "too many rendering commands");
    double phase = finite(transition);
    if (phase < -1 || phase > 1) rb_raise(error_class(), "invalid transition value");
    double fuzz = finite(vague);
    const lunetile::Raster* mask_image = NIL_P(mask) ? nullptr : lunetile_bitmap_raster(mask);
    frame->fill(0, 0, frame->width(), frame->height(), {0, 0, 0, 255});
    Clip full{0, 0, frame->width(), frame->height()}, active = full;
    bool inside_viewport = false;
    lunetile::Raster* drawing = frame.get();
    for (long i = 0; i < RARRAY_LEN(commands); ++i) {
        VALUE item = rb_ary_entry(commands, i);
        Check_Type(item, T_ARRAY);
        int kind = number(rb_ary_entry(item, 0));
        if (kind == 1) {
            if (RARRAY_LEN(item) != 2 || inside_viewport) rb_raise(error_class(), "invalid viewport command");
            active = intersect(full, rectangle(rb_ary_entry(item, 1))); inside_viewport = true;
            group->fill(active.x, active.y, active.w, active.h, {0, 0, 0, 0});
            drawing = group.get();
        } else if (kind == 2) {
            if (RARRAY_LEN(item) != 4 || !inside_viewport) rb_raise(error_class(), "invalid viewport end");
            VALUE tint = rb_ary_entry(item, 2), tone = rb_ary_entry(item, 3);
            if (!NIL_P(tone)) group->tone(active.x, active.y, active.w, active.h,
                property(tone, "red"), property(tone, "green"), property(tone, "blue"), property(tone, "gray"));
            frame->blit_clipped(0, 0, frame->width(), frame->height(), *group, 0, 0, group->width(), group->height(), 255, false,
                                active.x, active.y, active.w, active.h);
            if (!NIL_P(tint)) {
                auto channel = [](double n) { return uint8_t(std::lround(std::clamp(n, 0.0, 255.0))); };
                lunetile::Pixel color{channel(property(tint, "red")), channel(property(tint, "green")),
                                      channel(property(tint, "blue")), channel(property(tint, "alpha"))};
                frame->overlay(active.x, active.y, active.w, active.h, color);
            }
            drawing = frame.get();
            active = full; inside_viewport = false;
        } else if (kind == 0) {
            if (RARRAY_LEN(item) != 22) rb_raise(error_class(), "invalid bitmap draw command");
            double zx = finite(rb_ary_entry(item, 10)), zy = finite(rb_ary_entry(item, 11));
            double angle = finite(rb_ary_entry(item, 12)), ox = finite(rb_ary_entry(item, 13)), oy = finite(rb_ary_entry(item, 14));
            int blend_mode = number(rb_ary_entry(item, 16));
            if (blend_mode < 0 || blend_mode > 2) rb_raise(error_class(), "invalid blend mode");
            if (zx == 0 || zy == 0) continue;
            Clip clip = active;
            if (!NIL_P(rb_ary_entry(item, 20))) clip = intersect(clip, rectangle(rb_ary_entry(item, 20)));
            double x = finite(rb_ary_entry(item, 2)), y = finite(rb_ary_entry(item, 3));
            double w = finite(rb_ary_entry(item, 4)), h = finite(rb_ary_entry(item, 5));
            int sx = number(rb_ary_entry(item, 6)), sy = number(rb_ary_entry(item, 7));
            int sw = number(rb_ary_entry(item, 8)), sh = number(rb_ary_entry(item, 9));
            lunetile::QuadEffect effects;
            effects.opacity = std::clamp(number(rb_ary_entry(item, 15)), 0, 255);
            effects.blend = blend_mode; effects.bush_depth = number(rb_ary_entry(item, 19));
            effects.mirror = RTEST(rb_ary_entry(item, 21));
            VALUE color = rb_ary_entry(item, 17), tone = rb_ary_entry(item, 18);
            if (!NIL_P(color)) {
                auto channel = [](double n) { return uint8_t(std::lround(std::clamp(n, 0.0, 255.0))); };
                effects.color = {channel(property(color, "red")), channel(property(color, "green")),
                                 channel(property(color, "blue")), channel(property(color, "alpha"))};
            }
            if (!NIL_P(tone)) {
                effects.tone_red = property(tone, "red"); effects.tone_green = property(tone, "green");
                effects.tone_blue = property(tone, "blue"); effects.tone_gray = property(tone, "gray");
            }
            const auto* source = lunetile_bitmap_raster(rb_ary_entry(item, 1));
            char error[256] = {};
            try {
                if (angle == 0 && zx > 0 && zy > 0 && !effects.blend && effects.bush_depth <= 0 && !effects.color.a &&
                    effects.tone_red == 0 && effects.tone_green == 0 && effects.tone_blue == 0 && effects.tone_gray == 0) {
                    drawing->blit_clipped(bounded(x - ox * zx), bounded(y - oy * zy), bounded(w * zx), bounded(h * zy),
                        *source, sx, sy, sw, sh, uint8_t(effects.opacity), effects.mirror, clip.x, clip.y, clip.w, clip.h);
                } else drawing->quad(x, y, w, h, zx, zy, angle, ox, oy, *source, sx, sy, sw, sh, effects, clip.x, clip.y, clip.w, clip.h);
            }
            catch (const std::exception& ex) { ruby_snprintf(error, sizeof(error), "%s", ex.what()); }
            if (*error) rb_raise(error_class(), "%s", error);
        } else rb_raise(error_class(), "unsupported Android render command %d", kind);
    }
    if (inside_viewport) rb_raise(error_class(), "viewport command is not closed");
    last_output = frame.get();
    if (phase >= 0 && frozen) {
        *transitioned = *frame;
        for (size_t i = 0, n = size_t(frame->width()) * frame->height(); i < n; ++i) {
            auto& dst = transitioned->data()[i]; const auto& old = frozen->data()[i];
            double factor = phase;
            if (mask_image && fuzz >= 0) {
                int x = int(i % frame->width()), y = int(i / frame->width());
                int mx = int(int64_t(x) * mask_image->width() / frame->width());
                int my = int(int64_t(y) * mask_image->height() / frame->height());
                factor = lunetile::transition_factor(phase, mask_image->at(mx, my).r, fuzz);
            }
            dst.r = uint8_t(std::round(old.r * (1 - factor) + dst.r * factor));
            dst.g = uint8_t(std::round(old.g * (1 - factor) + dst.g * factor));
            dst.b = uint8_t(std::round(old.b * (1 - factor) + dst.b * factor));
        }
        last_output = transitioned.get();
        if (phase == 1) frozen.reset();
    }
    if (!output || !output->present(*last_output)) {
        if (!wait_handler || !wait_handler()) rb_raise(rb_eInterrupt, "Android game stopped");
        if (!output || !output->present(*last_output)) rb_raise(error_class(), "Android output surface unavailable");
    }
    return Qnil;
}
VALUE begin_frame(VALUE) {
    if (wait_handler && !wait_handler()) rb_raise(rb_eInterrupt, "Android game stopped");
    if (deadline == std::chrono::steady_clock::time_point{}) deadline = std::chrono::steady_clock::now();
    return Qnil;
}
VALUE tick(VALUE) {
    ++frame_count;
    auto now = std::chrono::steady_clock::now();
    deadline += std::chrono::nanoseconds(1000000000LL / frame_rate);
    if (deadline > now) std::this_thread::sleep_until(deadline);
    else if (!stock120 || now - deadline > std::chrono::milliseconds(250)) deadline = now;
    return Qnil;
}
VALUE freeze_frame(VALUE) {
    char error[256] = {};
    try { frozen = std::make_unique<lunetile::Raster>(*frame); }
    catch (const std::exception& ex) { ruby_snprintf(error, sizeof(error), "%s", ex.what()); }
    if (*error) rb_raise(error_class(), "%s", error);
    return Qnil;
}
VALUE width(VALUE) { return INT2NUM(frame->width()); }
VALUE height(VALUE) { return INT2NUM(frame->height()); }
VALUE count(VALUE) { return LONG2NUM(frame_count); }
VALUE scene_pixel(VALUE, VALUE x, VALUE y) {
    int px = NUM2INT(x), py = NUM2INT(y);
    if (px < 0 || py < 0 || px >= frame->width() || py >= frame->height()) rb_raise(rb_eArgError, "Pixel is outside the scene");
    auto p = frame->at(px, py);
    return rb_ary_new_from_args(4, INT2NUM(p.r), INT2NUM(p.g), INT2NUM(p.b), INT2NUM(p.a));
}
VALUE output_pixel(VALUE, VALUE x, VALUE y) {
    int px = NUM2INT(x), py = NUM2INT(y);
    if (!last_output || px < 0 || py < 0 || px >= last_output->width() || py >= last_output->height()) rb_raise(rb_eArgError, "Pixel is outside the output");
    auto p = last_output->at(px, py);
    return rb_ary_new_from_args(4, INT2NUM(p.r), INT2NUM(p.g), INT2NUM(p.b), INT2NUM(p.a));
}
VALUE set_count(VALUE, VALUE n) { frame_count = NUM2LONG(n); return n; }
VALUE rate(VALUE) { return INT2NUM(frame_rate); }
VALUE set_rate(VALUE, VALUE n) { int r = NUM2INT(n); if (stock120 && r != 120) rb_raise(rb_eArgError, "120 Hz timing requires frame_rate 120"); if (r < 1 || r > 120) rb_raise(rb_eArgError, "frame rate must be 1..120"); frame_rate = r; deadline = {}; return n; }
VALUE reset(VALUE) { deadline = {}; return Qnil; }
VALUE should_render(VALUE) {
    if (!stock120 || deadline == std::chrono::steady_clock::time_point{}) return Qtrue;
    return std::chrono::steady_clock::now() - deadline < std::chrono::nanoseconds(1000000000LL / 120) ? Qtrue : Qfalse;
}
VALUE high(VALUE) { return stock120 ? Qtrue : Qfalse; }
}
void lunetile_graphics_set_wait_handler(bool (*handler)()) { wait_handler = handler; }
void lunetile_init_graphics(LunetileSurface* surface, int width_value, int height_value, bool high_refresh) {
    frame = std::make_unique<lunetile::Raster>(width_value, height_value);
    group = std::make_unique<lunetile::Raster>(width_value, height_value);
    transitioned = std::make_unique<lunetile::Raster>(width_value, height_value);
    last_output = frame.get();
    frozen.reset(); frame_count = 0; stock120 = high_refresh; frame_rate = high_refresh ? 120 : 40; deadline = {}; output = surface;
    VALUE graphics = rb_define_module("Graphics");
    rb_define_singleton_method(graphics, "__present", RUBY_METHOD_FUNC(present), 4);
    rb_define_singleton_method(graphics, "__begin", RUBY_METHOD_FUNC(begin_frame), 0);
    rb_define_singleton_method(graphics, "__tick", RUBY_METHOD_FUNC(tick), 0);
    rb_define_singleton_method(graphics, "__freeze", RUBY_METHOD_FUNC(freeze_frame), 0);
    rb_define_singleton_method(graphics, "width", RUBY_METHOD_FUNC(width), 0);
    rb_define_singleton_method(graphics, "height", RUBY_METHOD_FUNC(height), 0);
    rb_define_singleton_method(graphics, "frame_count", RUBY_METHOD_FUNC(count), 0);
    rb_define_singleton_method(graphics, "__scene_pixel", RUBY_METHOD_FUNC(scene_pixel), 2);
    rb_define_singleton_method(graphics, "__output_pixel", RUBY_METHOD_FUNC(output_pixel), 2);
    rb_define_singleton_method(graphics, "frame_count=", RUBY_METHOD_FUNC(set_count), 1);
    rb_define_singleton_method(graphics, "frame_rate", RUBY_METHOD_FUNC(rate), 0);
    rb_define_singleton_method(graphics, "frame_rate=", RUBY_METHOD_FUNC(set_rate), 1);
    rb_define_singleton_method(graphics, "frame_reset", RUBY_METHOD_FUNC(reset), 0);
    rb_define_singleton_method(graphics, "__should_render", RUBY_METHOD_FUNC(should_render), 0);
    rb_define_singleton_method(graphics, "__stock120", RUBY_METHOD_FUNC(high), 0);
}
