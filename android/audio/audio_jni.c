#include "audio_jni.h"
#include <ruby.h>
#include <ruby/encoding.h>
#include <limits.h>

static JavaVM *vm;
static jobject bridge;
static jmethodID play_method, stop_method, fade_method;

static JNIEnv *environment(void) {
    JNIEnv *env = NULL;
    if (!vm || (*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK)
        rb_raise(rb_eRuntimeError, "Audio thread is not attached to Java");
    return env;
}

static void check_java(JNIEnv *env) {
    if (!(*env)->ExceptionCheck(env)) return;
    jthrowable error = (*env)->ExceptionOccurred(env);
    (*env)->ExceptionClear(env);
    jclass type = (*env)->FindClass(env, "java/lang/Throwable");
    jmethodID to_string = type ? (*env)->GetMethodID(env, type, "toString", "()Ljava/lang/String;") : NULL;
    jstring message = to_string ? (*env)->CallObjectMethod(env, error, to_string) : NULL;
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); message = NULL; }
    const char *text = message ? (*env)->GetStringUTFChars(env, message, NULL) : NULL;
    VALUE ruby_message = rb_sprintf("Android audio error: %s", text ? text : "Java exception");
    if (text) (*env)->ReleaseStringUTFChars(env, message, text);
    if (message) (*env)->DeleteLocalRef(env, message);
    if (type) (*env)->DeleteLocalRef(env, type);
    (*env)->DeleteLocalRef(env, error);
    rb_exc_raise(rb_exc_new_str(rb_eRuntimeError, ruby_message));
}

static VALUE play(int argc, VALUE *argv, int channel) {
    VALUE path, volume, pitch;
    rb_scan_args(argc, argv, "12", &path, &volume, &pitch);
    StringValue(path);
    path = rb_str_export_to_enc(path, rb_utf8_encoding());
    if (rb_enc_str_coderange(path) == ENC_CODERANGE_BROKEN) rb_raise(rb_eEncodingError, "Invalid UTF-8 audio path");
    int vol = argc < 2 ? 100 : NUM2INT(rb_to_int(volume));
    int rate = argc < 3 ? 100 : NUM2INT(rb_to_int(pitch));
    if (vol < 0) vol = 0; if (vol > 100) vol = 100;
    if (rate < 50) rate = 50; if (rate > 150) rate = 150;
    if (RSTRING_LEN(path) > INT_MAX) rb_raise(rb_eArgError, "Audio path is too long");
    JNIEnv *env = environment();
    jbyteArray bytes = (*env)->NewByteArray(env, (jsize)RSTRING_LEN(path));
    check_java(env);
    (*env)->SetByteArrayRegion(env, bytes, 0, (jsize)RSTRING_LEN(path), (const jbyte *)RSTRING_PTR(path));
    check_java(env);
    (*env)->CallVoidMethod(env, bridge, play_method, channel, bytes, vol, rate);
    (*env)->DeleteLocalRef(env, bytes);
    check_java(env);
    return Qnil;
}
static VALUE stop(int channel) {
    JNIEnv *env = environment();
    (*env)->CallVoidMethod(env, bridge, stop_method, channel);
    check_java(env);
    return Qnil;
}
static VALUE fade(VALUE milliseconds, int channel) {
    int value = NUM2INT(rb_to_int(milliseconds));
    if (value < 0) value = 0;
    JNIEnv *env = environment();
    (*env)->CallVoidMethod(env, bridge, fade_method, channel, value);
    check_java(env);
    return Qnil;
}
#define AUDIO_METHODS(name, id) \
    static VALUE name##_play(int argc, VALUE *argv, VALUE self) { (void)self; return play(argc, argv, id); } \
    static VALUE name##_stop(VALUE self) { (void)self; return stop(id); }
AUDIO_METHODS(bgm, 0)
AUDIO_METHODS(bgs, 1)
AUDIO_METHODS(me, 2)
AUDIO_METHODS(se, 3)
#define FADE_METHOD(name, id) static VALUE name##_fade(VALUE self, VALUE time) { (void)self; return fade(time, id); }
FADE_METHOD(bgm, 0)
FADE_METHOD(bgs, 1)
FADE_METHOD(me, 2)

int lunetile_audio_register(JNIEnv *env, jobject object) {
    if (!object || bridge || (*env)->GetJavaVM(env, &vm) != JNI_OK) return 0;
    jclass klass = (*env)->GetObjectClass(env, object);
    play_method = (*env)->GetMethodID(env, klass, "play", "(I[BII)V");
    stop_method = (*env)->GetMethodID(env, klass, "stop", "(I)V");
    fade_method = (*env)->GetMethodID(env, klass, "fade", "(II)V");
    (*env)->DeleteLocalRef(env, klass);
    if (!play_method || !stop_method || !fade_method || (*env)->ExceptionCheck(env)) return 0;
    bridge = (*env)->NewGlobalRef(env, object);
    if (!bridge) return 0;
    VALUE audio = rb_define_module("Audio");
#define REGISTER(name) rb_define_singleton_method(audio, #name "_play", RUBY_METHOD_FUNC(name##_play), -1); rb_define_singleton_method(audio, #name "_stop", RUBY_METHOD_FUNC(name##_stop), 0)
    REGISTER(bgm); REGISTER(bgs); REGISTER(me); REGISTER(se);
    rb_define_singleton_method(audio, "bgm_fade", RUBY_METHOD_FUNC(bgm_fade), 1);
    rb_define_singleton_method(audio, "bgs_fade", RUBY_METHOD_FUNC(bgs_fade), 1);
    rb_define_singleton_method(audio, "me_fade", RUBY_METHOD_FUNC(me_fade), 1);
#undef REGISTER
    return 1;
}

void lunetile_audio_unregister(JNIEnv *env) {
    if (bridge) (*env)->DeleteGlobalRef(env, bridge);
    bridge = NULL; vm = NULL; play_method = NULL; stop_method = NULL; fade_method = NULL;
}
