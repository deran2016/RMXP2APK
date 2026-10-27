#include <jni.h>
#include <ruby.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <stdexcept>
#include "../core/rgss_data.h"
extern "C" {
#include "../audio/audio_jni.h"
#include "../input/input_jni.h"
}
#include "bitmap.h"
#include "graphics.h"
#include "graphics_builtin.h"
#include "ruby-probe-options.h"

namespace {
LunetileSurface surface;
std::mutex state_mutex;
std::condition_variable state_changed;
bool has_surface = false, paused = true, stopping = false, started = false, running = false, at_gate = false;

bool wait_for_frame() {
    std::unique_lock<std::mutex> lock(state_mutex);
    at_gate = true;
    state_changed.notify_all();
    state_changed.wait(lock, [] { return stopping || (!paused && has_surface); });
    at_gate = false;
    state_changed.notify_all();
    return !stopping;
}
std::string utf8(JNIEnv* env, jstring value) {
    if (!value) throw std::runtime_error("Missing game configuration");
    jclass string_class = env->FindClass("java/lang/String");
    jmethodID get_bytes = string_class ? env->GetMethodID(string_class, "getBytes", "(Ljava/lang/String;)[B") : nullptr;
    jstring charset = env->NewStringUTF("UTF-8");
    jbyteArray bytes = get_bytes && charset ? static_cast<jbyteArray>(env->CallObjectMethod(value, get_bytes, charset)) : nullptr;
    if (charset) env->DeleteLocalRef(charset);
    if (string_class) env->DeleteLocalRef(string_class);
    if (env->ExceptionCheck() || !bytes) throw std::runtime_error("Cannot decode game configuration");
    jsize size = env->GetArrayLength(bytes);
    std::string result(size_t(size), '\0');
    env->GetByteArrayRegion(bytes, 0, size, reinterpret_cast<jbyte*>(result.data()));
    env->DeleteLocalRef(bytes);
    if (env->ExceptionCheck() || result.find('\0') != std::string::npos) throw std::runtime_error("Invalid game configuration");
    return result;
}
jstring java_string(JNIEnv* env, const std::string& value) {
    jclass string_class = env->FindClass("java/lang/String");
    jmethodID constructor = string_class ? env->GetMethodID(string_class, "<init>", "([BLjava/lang/String;)V") : nullptr;
    jstring charset = env->NewStringUTF("UTF-8");
    jbyteArray bytes = env->NewByteArray(jsize(value.size()));
    if (bytes) env->SetByteArrayRegion(bytes, 0, jsize(value.size()), reinterpret_cast<const jbyte*>(value.data()));
    jstring result = constructor && charset && bytes && !env->ExceptionCheck() ?
        static_cast<jstring>(env->NewObject(string_class, constructor, bytes, charset)) : nullptr;
    if (bytes) env->DeleteLocalRef(bytes);
    if (charset) env->DeleteLocalRef(charset);
    if (string_class) env->DeleteLocalRef(string_class);
    return result;
}
struct Boot {
    JNIEnv* env;
    jobject input;
    jobject audio;
    const std::string *root, *saves, *encoding, *scripts;
    int width, height;
    bool high_refresh;
};
VALUE boot(VALUE arg) {
    auto* b = reinterpret_cast<Boot*>(arg);
    lunetile_android_init_data(rb_utf8_str_new(b->encoding->data(), long(b->encoding->size())));
    lunetile_android_init_project(rb_utf8_str_new(b->root->data(), long(b->root->size())));
    lunetile_android_init_saves(rb_utf8_str_new(b->saves->data(), long(b->saves->size())));
    lunetile_init_bitmap();
    lunetile_init_graphics(&surface, b->width, b->height, b->high_refresh);
    lunetile_graphics_set_wait_handler(wait_for_frame);
    if (!lunetile_input_register(b->env, b->input, b->high_refresh ? 3 : 1)) {
        if (b->env->ExceptionCheck()) b->env->ExceptionClear();
        rb_raise(rb_eRuntimeError, "Cannot bind Android input");
    }
    if (!lunetile_audio_register(b->env, b->audio)) {
        if (b->env->ExceptionCheck()) b->env->ExceptionClear();
        rb_raise(rb_eRuntimeError, "Cannot bind Android audio");
    }
    rb_eval_string(graphics_support);
    VALUE module = rb_const_get(rb_cObject, rb_intern("RGSSXP"));
    return rb_funcall(module, rb_intern("run_scripts"), 1, rb_utf8_str_new(b->scripts->data(), long(b->scripts->size())));
}
VALUE error_message(VALUE error) { return rb_funcall(error, rb_intern("full_message"), 0); }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_lunetile_host_NativeGameEngine_nativeSurface(JNIEnv* env, jclass, jobject value) {
    if (!value) {
        { std::lock_guard<std::mutex> lock(state_mutex); has_surface = false; }
        surface.clear(); state_changed.notify_all(); return JNI_TRUE;
    }
    if (!surface.bind(env, value)) return JNI_FALSE;
    { std::lock_guard<std::mutex> lock(state_mutex); has_surface = true; }
    state_changed.notify_all(); return JNI_TRUE;
}
extern "C" JNIEXPORT void JNICALL
Java_org_lunetile_host_NativeGameEngine_nativePause(JNIEnv* env, jclass, jboolean value) {
    std::unique_lock<std::mutex> lock(state_mutex);
    paused = value == JNI_TRUE;
    state_changed.notify_all();
    if (paused && running && !at_gate && !stopping &&
        !state_changed.wait_for(lock, std::chrono::seconds(3), [] { return at_gate || !running || stopping; })) {
        jclass error = env->FindClass("java/lang/IllegalStateException");
        if (error) env->ThrowNew(error, "Game did not reach a frame boundary while pausing");
    }
}
extern "C" JNIEXPORT void JNICALL
Java_org_lunetile_host_NativeGameEngine_nativeStop(JNIEnv*, jclass) {
    { std::lock_guard<std::mutex> lock(state_mutex); stopping = true; }
    state_changed.notify_all();
}
extern "C" JNIEXPORT jstring JNICALL
Java_org_lunetile_host_NativeGameEngine_nativeRun(JNIEnv* env, jclass, jclass codec, jobject input, jobject audio,
                                                  jstring root, jstring saves, jstring encoding, jstring scripts,
                                                  jint width, jint height, jboolean high_refresh) {
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        if (started) return java_string(env, "Ruby can be started only once per process");
        started = true; running = true;
    }
    std::string failure;
    bool initialized = false, codec_bound = false;
    int status = 0;
    try {
        std::string root_value = utf8(env, root), saves_value = utf8(env, saves);
        std::string encoding_value = utf8(env, encoding), scripts_value = utf8(env, scripts);
        if (!input || !audio || !codec || width <= 0 || height <= 0) throw std::runtime_error("Invalid Android game configuration");
        if (!wait_for_frame()) throw std::runtime_error("Game stopped before startup");
        codec_bound = lunetile_bind_bitmap_codec(env, codec);
        if (!codec_bound) throw std::runtime_error("Cannot bind Android bitmap decoder");
        int argc = 1;
        char program[] = "lunetile";
        char* arguments[] = {program, nullptr};
        char** argv = arguments;
        ruby_sysinit(&argc, &argv);
        RUBY_INIT_STACK;
        if (ruby_setup() != 0) throw std::runtime_error("ruby_setup failed");
        initialized = true;
        if (ruby_probe_init_builtins()) throw std::runtime_error("Ruby builtin initialization failed");
        Boot context{env, input, audio, &root_value, &saves_value, &encoding_value, &scripts_value, width, height, high_refresh == JNI_TRUE};
        rb_protect(boot, reinterpret_cast<VALUE>(&context), &status);
        if (status) {
            bool stopped;
            { std::lock_guard<std::mutex> lock(state_mutex); stopped = stopping; }
            if (!(stopped && rb_obj_is_kind_of(rb_errinfo(), rb_eInterrupt))) {
                int formatting = 0;
                VALUE message = rb_protect(error_message, rb_errinfo(), &formatting);
                if (!formatting && RB_TYPE_P(message, T_STRING)) failure.assign(RSTRING_PTR(message), size_t(RSTRING_LEN(message)));
                else failure = "Ruby game failed; error text unavailable";
            }
        }
    } catch (const std::exception& ex) {
        { std::lock_guard<std::mutex> lock(state_mutex); if (!stopping) failure = ex.what(); }
    }
    if (env->ExceptionCheck()) { env->ExceptionClear(); if (failure.empty()) failure = "Android JNI operation failed"; }
    lunetile_graphics_set_wait_handler(nullptr);
    if (initialized) ruby_cleanup(status ? 1 : 0);
    lunetile_audio_unregister(env);
    lunetile_input_unregister(env);
    if (codec_bound) lunetile_unbind_bitmap_codec(env);
    { std::lock_guard<std::mutex> lock(state_mutex); running = false; at_gate = false; }
    state_changed.notify_all();
    return failure.empty() ? nullptr : java_string(env, failure);
}
