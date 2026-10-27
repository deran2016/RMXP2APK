#include "bitmap.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {
JavaVM* vm = nullptr;
jclass codec_class = nullptr;
jmethodID decode_method = nullptr;
jmethodID text_method = nullptr;
struct Bitmap { std::unique_ptr<lunetile::Raster> image; };
void bitmap_free(void* p) { delete static_cast<Bitmap*>(p); }
size_t bitmap_size(const void* p) {
    auto* b = static_cast<const Bitmap*>(p);
    return sizeof(Bitmap) + (b->image ? size_t(b->image->width()) * b->image->height() * sizeof(lunetile::Pixel) : 0);
}
const rb_data_type_t bitmap_type = {"LunetileAndroid::Bitmap", {nullptr, bitmap_free, bitmap_size}, nullptr, nullptr, RUBY_TYPED_FREE_IMMEDIATELY};
VALUE error_class() { return rb_const_get(rb_cObject, rb_intern("RGSSError")); }
Bitmap* bitmap(VALUE value) { Bitmap* b; TypedData_Get_Struct(value, Bitmap, &bitmap_type, b); return b; }
lunetile::Raster& live(VALUE value) {
    Bitmap* b = bitmap(value);
    if (!b->image) rb_raise(error_class(), "disposed or uninitialized Bitmap");
    return *b->image;
}
VALUE alloc(VALUE klass) {
    Bitmap* value = new (std::nothrow) Bitmap;
    if (!value) rb_memerror();
    return TypedData_Wrap_Struct(klass, &bitmap_type, value);
}
void image_error(const char* message) { rb_raise(error_class(), "%s", message); }
VALUE empty(VALUE self, VALUE width, VALUE height) {
    int w = NUM2INT(width), h = NUM2INT(height);
    char error[256] = {};
    try { bitmap(self)->image = std::make_unique<lunetile::Raster>(w, h); }
    catch (const std::exception& ex) { std::snprintf(error, sizeof(error), "%s", ex.what()); }
    if (*error) image_error(error);
    return self;
}
std::unique_ptr<lunetile::Raster> decode_image(const char* bytes, size_t length) {
    if (!vm || !codec_class || !decode_method) throw std::runtime_error("Android bitmap decoder is not bound");
    if (length > size_t(std::numeric_limits<jsize>::max())) throw std::runtime_error("image is too large");
    JNIEnv* env = nullptr;
    bool attached = false;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) throw std::runtime_error("cannot attach bitmap decoder thread");
        attached = true;
    }
    if (!env) throw std::runtime_error("cannot access JNI environment");
    jbyteArray input = env->NewByteArray(jsize(length));
    if (input) env->SetByteArrayRegion(input, 0, jsize(length), reinterpret_cast<const jbyte*>(bytes));
    jintArray decoded = input && !env->ExceptionCheck() ? static_cast<jintArray>(env->CallStaticObjectMethod(codec_class, decode_method, input)) : nullptr;
    std::unique_ptr<lunetile::Raster> result;
    if (!env->ExceptionCheck() && decoded && env->GetArrayLength(decoded) >= 2) {
        jint* values = env->GetIntArrayElements(decoded, nullptr);
        if (values) {
            int w = values[0], h = values[1];
            try {
                if (w > 0 && h > 0 && int64_t(w) * h == int64_t(env->GetArrayLength(decoded)) - 2) {
                    result = std::make_unique<lunetile::Raster>(w, h);
                    for (int64_t i = 0; i < int64_t(w) * h; ++i) {
                        uint32_t c = uint32_t(values[i + 2]);
                        result->data()[i] = {uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c), uint8_t(c >> 24)};
                    }
                }
            } catch (...) { result.reset(); }
            env->ReleaseIntArrayElements(decoded, values, JNI_ABORT);
        }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (decoded) env->DeleteLocalRef(decoded);
    if (input) env->DeleteLocalRef(input);
    if (attached) vm->DetachCurrentThread();
    if (!result) throw std::runtime_error("image decoding failed or image exceeds limits");
    return result;
}
VALUE decode(VALUE self, VALUE bytes) {
    StringValue(bytes);
    char error[256] = {};
    try { bitmap(self)->image = decode_image(RSTRING_PTR(bytes), size_t(RSTRING_LEN(bytes))); }
    catch (const std::exception& ex) { std::snprintf(error, sizeof(error), "%s", ex.what()); }
    if (*error) image_error(error);
    return self;
}
VALUE copy(VALUE self, VALUE other) {
    auto& source = live(other);
    char error[256] = {};
    try { bitmap(self)->image = std::make_unique<lunetile::Raster>(source); }
    catch (const std::exception& ex) { std::snprintf(error, sizeof(error), "%s", ex.what()); }
    if (*error) image_error(error);
    return self;
}
VALUE width(VALUE self) { return INT2NUM(live(self).width()); }
VALUE height(VALUE self) { return INT2NUM(live(self).height()); }
VALUE dispose(VALUE self) { bitmap(self)->image.reset(); return Qnil; }
VALUE disposed(VALUE self) { return bitmap(self)->image ? Qfalse : Qtrue; }
uint8_t component(VALUE color, const char* name) {
    double value = NUM2DBL(rb_funcall(color, rb_intern(name), 0));
    if (!std::isfinite(value)) rb_raise(rb_eArgError, "non-finite color component");
    return uint8_t(std::clamp(value, 0.0, 255.0));
}
VALUE fill(VALUE self, VALUE x, VALUE y, VALUE w, VALUE h, VALUE color) {
    int px = NUM2INT(x), py = NUM2INT(y), pw = NUM2INT(w), ph = NUM2INT(h);
    lunetile::Pixel c{component(color, "red"), component(color, "green"), component(color, "blue"), component(color, "alpha")};
    live(self).fill(px, py, pw, ph, c);
    return Qnil;
}
VALUE blit(VALUE self, VALUE args) {
    Check_Type(args, T_ARRAY);
    if (RARRAY_LEN(args) != 10) rb_raise(rb_eArgError, "invalid blit arguments");
    int x = NUM2INT(rb_ary_entry(args, 0)), y = NUM2INT(rb_ary_entry(args, 1));
    int w = NUM2INT(rb_ary_entry(args, 2)), h = NUM2INT(rb_ary_entry(args, 3));
    VALUE source = rb_ary_entry(args, 4);
    int sx = NUM2INT(rb_ary_entry(args, 5)), sy = NUM2INT(rb_ary_entry(args, 6));
    int sw = NUM2INT(rb_ary_entry(args, 7)), sh = NUM2INT(rb_ary_entry(args, 8));
    int opacity = std::clamp(NUM2INT(rb_ary_entry(args, 9)), 0, 255);
    char error[256] = {};
    try { live(self).blit(x, y, w, h, live(source), sx, sy, sw, sh, uint8_t(opacity)); }
    catch (const std::exception& ex) { std::snprintf(error, sizeof(error), "%s", ex.what()); }
    if (*error) image_error(error);
    return Qnil;
}
VALUE pixel(VALUE self, VALUE x, VALUE y) {
    auto& image = live(self);
    int px = NUM2INT(x), py = NUM2INT(y);
    lunetile::Pixel c = px < 0 || py < 0 || px >= image.width() || py >= image.height() ? lunetile::Pixel{0,0,0,0} : image.at(px, py);
    VALUE values[] = {INT2NUM(c.r), INT2NUM(c.g), INT2NUM(c.b), INT2NUM(c.a)};
    return rb_class_new_instance(4, values, rb_const_get(rb_cObject, rb_intern("Color")));
}
VALUE set_pixel(VALUE self, VALUE x, VALUE y, VALUE color) {
    int px = NUM2INT(x), py = NUM2INT(y);
    lunetile::Pixel c{component(color, "red"), component(color, "green"), component(color, "blue"), component(color, "alpha")};
    live(self).fill(px, py, 1, 1, c);
    return Qnil;
}
VALUE bytes(VALUE self) {
    auto& image = live(self);
    return rb_str_new(reinterpret_cast<const char*>(image.data()), long(size_t(image.width()) * image.height() * sizeof(lunetile::Pixel)));
}
struct TextOptions { int font_size, bold, italic, color, x, y, width, height, align, measure; };
std::pair<int, int> render_text(lunetile::Raster& image, const char* text_bytes, size_t text_length,
                                const char* font_bytes, size_t font_length, const TextOptions& options) {
    if (!vm || !codec_class || !text_method) throw std::runtime_error("Android text renderer is not bound");
    if (text_length > size_t(std::numeric_limits<jsize>::max()) || font_length > size_t(std::numeric_limits<jsize>::max()))
        throw std::runtime_error("text is too large");
    JNIEnv* env = nullptr;
    bool attached = false;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) throw std::runtime_error("cannot attach text renderer thread");
        attached = true;
    }
    if (!env) throw std::runtime_error("cannot access JNI environment");
    jsize count = image.width() * image.height();
    jintArray pixels = env->NewIntArray(count);
    jbyteArray text_data = !env->ExceptionCheck() ? env->NewByteArray(jsize(text_length)) : nullptr;
    jbyteArray font_data = !env->ExceptionCheck() ? env->NewByteArray(jsize(font_length)) : nullptr;
    jintArray params = !env->ExceptionCheck() ? env->NewIntArray(12) : nullptr;
    bool ready = false;
    if (!env->ExceptionCheck() && pixels && text_data && font_data && params) {
        jint* data = env->GetIntArrayElements(pixels, nullptr);
        if (data) {
            for (jsize i = 0; i < count; ++i) {
                auto p = image.data()[i];
                data[i] = jint(uint32_t(p.a) << 24 | uint32_t(p.r) << 16 | uint32_t(p.g) << 8 | p.b);
            }
            env->ReleaseIntArrayElements(pixels, data, 0);
            env->SetByteArrayRegion(text_data, 0, jsize(text_length), reinterpret_cast<const jbyte*>(text_bytes));
            env->SetByteArrayRegion(font_data, 0, jsize(font_length), reinterpret_cast<const jbyte*>(font_bytes));
            jint values[] = {image.width(), image.height(), options.font_size, options.bold, options.italic, options.color,
                             options.x, options.y, options.width, options.height, options.align, options.measure};
            env->SetIntArrayRegion(params, 0, 12, values);
            ready = !env->ExceptionCheck();
        }
    }
    jintArray result = ready ?
        static_cast<jintArray>(env->CallStaticObjectMethod(codec_class, text_method, pixels, text_data, font_data, params)) : nullptr;
    std::pair<int, int> dimensions{-1, -1};
    if (!env->ExceptionCheck() && result && env->GetArrayLength(result) == (options.measure ? 2 : count + 2)) {
        jint* values = env->GetIntArrayElements(result, nullptr);
        if (values) {
            dimensions = {values[0], values[1]};
            if (!options.measure) for (jsize i = 0; i < count; ++i) {
                uint32_t c = uint32_t(values[i + 2]);
                image.data()[i] = {uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c), uint8_t(c >> 24)};
            }
            env->ReleaseIntArrayElements(result, values, JNI_ABORT);
        }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (result) env->DeleteLocalRef(result);
    if (pixels) env->DeleteLocalRef(pixels);
    if (text_data) env->DeleteLocalRef(text_data);
    if (font_data) env->DeleteLocalRef(font_data);
    if (params) env->DeleteLocalRef(params);
    if (attached) vm->DetachCurrentThread();
    if (dimensions.first < 0 || dimensions.second < 0) throw std::runtime_error("Android text rendering failed");
    return dimensions;
}
VALUE text(VALUE self, VALUE content, VALUE font, VALUE args) {
    StringValue(content);
    Check_Type(args, T_ARRAY);
    if (RARRAY_LEN(args) != 6) rb_raise(rb_eArgError, "invalid text arguments");
    VALUE name = rb_funcall(font, rb_intern("name"), 0);
    StringValue(name);
    VALUE color = rb_funcall(font, rb_intern("color"), 0);
    uint32_t argb = uint32_t(component(color, "alpha")) << 24 | uint32_t(component(color, "red")) << 16 |
                    uint32_t(component(color, "green")) << 8 | component(color, "blue");
    TextOptions options{NUM2INT(rb_funcall(font, rb_intern("size"), 0)), RTEST(rb_funcall(font, rb_intern("bold"), 0)),
        RTEST(rb_funcall(font, rb_intern("italic"), 0)), int(argb), NUM2INT(rb_ary_entry(args, 0)), NUM2INT(rb_ary_entry(args, 1)),
        NUM2INT(rb_ary_entry(args, 2)), NUM2INT(rb_ary_entry(args, 3)), NUM2INT(rb_ary_entry(args, 4)), RTEST(rb_ary_entry(args, 5))};
    char error[256] = {};
    std::pair<int, int> dimensions{};
    try { dimensions = render_text(live(self), RSTRING_PTR(content), size_t(RSTRING_LEN(content)), RSTRING_PTR(name), size_t(RSTRING_LEN(name)), options); }
    catch (const std::exception& ex) { std::snprintf(error, sizeof(error), "%s", ex.what()); }
    if (*error) image_error(error);
    return options.measure ? rb_ary_new_from_args(2, INT2NUM(dimensions.first), INT2NUM(dimensions.second)) : Qnil;
}
VALUE transparent(VALUE self) {
    auto& image = live(self);
    for (size_t i = 0, n = size_t(image.width()) * image.height(); i < n; ++i)
        if (image.data()[i].a) return Qfalse;
    return Qtrue;
}
}
bool lunetile_bind_bitmap_codec(JNIEnv* env, jclass codec) {
    if (vm || !env || !codec || env->GetJavaVM(&vm) != JNI_OK) return false;
    codec_class = static_cast<jclass>(env->NewGlobalRef(codec));
    decode_method = codec_class ? env->GetStaticMethodID(codec_class, "decode", "([B)[I") : nullptr;
    text_method = codec_class && decode_method ? env->GetStaticMethodID(codec_class, "text", "([I[B[B[I)[I") : nullptr;
    if (!decode_method || !text_method) { if (codec_class) env->DeleteGlobalRef(codec_class); codec_class = nullptr; decode_method = text_method = nullptr; vm = nullptr; return false; }
    return true;
}
void lunetile_unbind_bitmap_codec(JNIEnv* env) {
    if (env && codec_class) env->DeleteGlobalRef(codec_class);
    codec_class = nullptr; decode_method = nullptr; text_method = nullptr; vm = nullptr;
}
void lunetile_init_bitmap() {
    rb_define_class("RGSSError", rb_eStandardError);
    VALUE klass = rb_define_class("Bitmap", rb_cObject);
    rb_define_alloc_func(klass, alloc);
    rb_define_method(klass, "__empty", RUBY_METHOD_FUNC(empty), 2);
    rb_define_method(klass, "__decode", RUBY_METHOD_FUNC(decode), 1);
    rb_define_method(klass, "__copy", RUBY_METHOD_FUNC(copy), 1);
    rb_define_method(klass, "width", RUBY_METHOD_FUNC(width), 0);
    rb_define_method(klass, "height", RUBY_METHOD_FUNC(height), 0);
    rb_define_method(klass, "dispose", RUBY_METHOD_FUNC(dispose), 0);
    rb_define_method(klass, "disposed?", RUBY_METHOD_FUNC(disposed), 0);
    rb_define_method(klass, "__fill", RUBY_METHOD_FUNC(fill), 5);
    rb_define_method(klass, "__blt", RUBY_METHOD_FUNC(blit), 1);
    rb_define_method(klass, "get_pixel", RUBY_METHOD_FUNC(pixel), 2);
    rb_define_method(klass, "set_pixel", RUBY_METHOD_FUNC(set_pixel), 3);
    rb_define_method(klass, "__pixel_bytes", RUBY_METHOD_FUNC(bytes), 0);
    rb_define_method(klass, "__transparent?", RUBY_METHOD_FUNC(transparent), 0);
    rb_define_method(klass, "__text", RUBY_METHOD_FUNC(text), 3);
}
const lunetile::Raster* lunetile_bitmap_raster(VALUE value) { return &live(value); }
