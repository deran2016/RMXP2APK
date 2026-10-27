#include <jni.h>
#include <ruby.h>
#include <stdatomic.h>
#include <stdio.h>
#include "../../runtime/ruby-probe-options.h"
#include "../input_jni.h"

static JNIEnv *probe_env;
static jobject activity;
static jmethodID stage_method;
static VALUE stage(VALUE self, VALUE number) {
    (void)self;
    (*probe_env)->CallVoidMethod(probe_env, activity, stage_method, NUM2INT(number));
    if ((*probe_env)->ExceptionCheck(probe_env)) { (*probe_env)->ExceptionClear(probe_env); rb_raise(rb_eRuntimeError, "UI input stage failed"); }
    return Qnil;
}
JNIEXPORT jstring JNICALL Java_org_lunetile_input_InputProbeActivity_runProbe(JNIEnv *env, jobject self, jobject state) {
    static atomic_flag used = ATOMIC_FLAG_INIT;
    if (atomic_flag_test_and_set(&used)) return (*env)->NewStringUTF(env, "FAIL: probe already used");
    int argc = 1; char name[] = "input-probe"; char *args[] = {name, NULL}; char **argv = args;
    ruby_sysinit(&argc, &argv);
    RUBY_INIT_STACK;
    if (ruby_setup() != 0) return (*env)->NewStringUTF(env, "FAIL: ruby_setup");
    if (ruby_probe_init_builtins() || !lunetile_input_register(env, state, 1)) {
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        ruby_cleanup(1); return (*env)->NewStringUTF(env, "FAIL: initialization");
    }
    probe_env = env; activity = self;
    jclass klass = (*env)->GetObjectClass(env, self);
    stage_method = (*env)->GetMethodID(env, klass, "stage", "(I)V");
    (*env)->DeleteLocalRef(env, klass);
    if (!stage_method || (*env)->ExceptionCheck(env)) {
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        lunetile_input_unregister(env); ruby_cleanup(1);
        return (*env)->NewStringUTF(env, "FAIL: stage method");
    }
    VALUE input = rb_const_get(rb_cObject, rb_intern("Input"));
    rb_define_singleton_method(input, "__probe_stage", RUBY_METHOD_FUNC(stage), 1);
    const char *script =
        "Input.__probe_stage(0); Input.update; raise 'up' unless Input.press?(Input::UP) && Input.trigger?(Input::UP) && Input.repeat?(Input::UP) && Input.dir4==8 && Input.dir8==8; "
        "Input.__probe_stage(1); Input.update; raise 'diagonal' unless Input.dir4==6 && Input.dir8==9; "
        "Input.__probe_stage(2); Input.update; raise 'focus release' unless Input.dir4==0 && !Input.press?(Input::UP); "
        "Input.__probe_stage(3); Input.update; raise 'short tap' unless Input.trigger?(Input::C); Input.update; raise 'tap release' if Input.press?(Input::C); "
        "Input.__probe_stage(4); Input.update; raise 'cancelled tap' if Input.trigger?(Input::B); "
        "Input.__probe_stage(0); Input.update; 15.times { Input.update; raise 'early repeat' if Input.repeat?(Input::UP) }; Input.update; raise 'repeat' unless Input.repeat?(Input::UP); "
        "begin; Input.press?(30); raise 'accepted invalid'; rescue ArgumentError; end; true";
    int status = 0; VALUE result = rb_eval_string_protect(script, &status);
    char message[512] = "PASS: Ruby Input JNI touch state and focus release";
    if (status || result != Qtrue) {
        VALUE exception = rb_errinfo(); VALUE text = NIL_P(exception) ? Qnil : rb_obj_as_string(exception);
        snprintf(message, sizeof(message), "FAIL: %s", NIL_P(text) ? "Ruby evaluation" : StringValueCStr(text));
        rb_set_errinfo(Qnil);
    }
    lunetile_input_unregister(env); ruby_cleanup(status ? 1 : 0);
    return (*env)->NewStringUTF(env, message);
}
