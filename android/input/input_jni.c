#include "input_jni.h"
#include <ruby.h>
#include <stdatomic.h>

static JavaVM *vm;
static jobject input_state;
static jmethodID update_method, press_method, trigger_method, repeat_method, dir4_method, dir8_method;
static atomic_int timing_scale = 1;

static JNIEnv *environment(void) {
    JNIEnv *env = NULL;
    if (!vm || (*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK)
        rb_raise(rb_eRuntimeError, "Input thread is not attached to Java");
    return env;
}
static void check_java(JNIEnv *env) {
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        rb_raise(rb_eRuntimeError, "Android input state operation failed");
    }
}
static int button(VALUE value) {
    int id = NUM2INT(value);
    if (id < 0 || id >= 30) rb_raise(rb_eArgError, "Unknown input button");
    return id;
}
static VALUE update(VALUE self) {
    (void)self; JNIEnv *env = environment();
    (*env)->CallVoidMethod(env, input_state, update_method, atomic_load(&timing_scale));
    check_java(env); return Qnil;
}
static VALUE query(VALUE value, jmethodID method) {
    int id = button(value); JNIEnv *env = environment();
    jboolean result = (*env)->CallBooleanMethod(env, input_state, method, id);
    check_java(env); return result ? Qtrue : Qfalse;
}
static VALUE press(VALUE self, VALUE value) { (void)self; return query(value, press_method); }
static VALUE trigger(VALUE self, VALUE value) { (void)self; return query(value, trigger_method); }
static VALUE repeat(VALUE self, VALUE value) { (void)self; return query(value, repeat_method); }
static VALUE direction(jmethodID method) {
    JNIEnv *env = environment(); jint result = (*env)->CallIntMethod(env, input_state, method);
    check_java(env); return INT2NUM(result);
}
static VALUE dir4(VALUE self) { (void)self; return direction(dir4_method); }
static VALUE dir8(VALUE self) { (void)self; return direction(dir8_method); }

int lunetile_input_register(JNIEnv *env, jobject state, int scale) {
    if (!state || input_state || (scale != 1 && scale != 3) || (*env)->GetJavaVM(env, &vm) != JNI_OK) return 0;
    jclass type = (*env)->GetObjectClass(env, state);
    update_method = (*env)->GetMethodID(env, type, "update", "(I)V");
    press_method = (*env)->GetMethodID(env, type, "press", "(I)Z");
    trigger_method = (*env)->GetMethodID(env, type, "trigger", "(I)Z");
    repeat_method = (*env)->GetMethodID(env, type, "repeat", "(I)Z");
    dir4_method = (*env)->GetMethodID(env, type, "dir4", "()I");
    dir8_method = (*env)->GetMethodID(env, type, "dir8", "()I");
    (*env)->DeleteLocalRef(env, type);
    if (!update_method || !press_method || !trigger_method || !repeat_method || !dir4_method || !dir8_method || (*env)->ExceptionCheck(env)) return 0;
    input_state = (*env)->NewGlobalRef(env, state);
    if (!input_state) return 0;
    atomic_store(&timing_scale, scale);
    VALUE input = rb_define_module("Input");
    const char *names[] = {"DOWN", "LEFT", "RIGHT", "UP", "A", "B", "C", "X", "Y", "Z", "L", "R", "SHIFT", "CTRL", "ALT", "F5", "F6", "F7", "F8", "F9"};
    const int ids[] = {2, 4, 6, 8, 11, 12, 13, 14, 15, 16, 17, 18, 21, 22, 23, 25, 26, 27, 28, 29};
    for (int i = 0; i < 20; ++i) rb_define_const(input, names[i], INT2NUM(ids[i]));
    rb_define_singleton_method(input, "update", RUBY_METHOD_FUNC(update), 0);
    rb_define_singleton_method(input, "press?", RUBY_METHOD_FUNC(press), 1);
    rb_define_singleton_method(input, "trigger?", RUBY_METHOD_FUNC(trigger), 1);
    rb_define_singleton_method(input, "repeat?", RUBY_METHOD_FUNC(repeat), 1);
    rb_define_singleton_method(input, "dir4", RUBY_METHOD_FUNC(dir4), 0);
    rb_define_singleton_method(input, "dir8", RUBY_METHOD_FUNC(dir8), 0);
    return 1;
}
void lunetile_input_set_scale(int scale) { if (scale == 1 || scale == 3) atomic_store(&timing_scale, scale); }
void lunetile_input_unregister(JNIEnv *env) {
    if (input_state) (*env)->DeleteGlobalRef(env, input_state);
    input_state = NULL; vm = NULL;
    update_method = press_method = trigger_method = repeat_method = dir4_method = dir8_method = NULL;
}
