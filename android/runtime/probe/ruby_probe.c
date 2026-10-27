#include <jni.h>
#include <ruby.h>
#include <stdatomic.h>
#include "../ruby-probe-options.h"

/* Isolated one-shot diagnostic; it is not the game runtime. */
JNIEXPORT jstring JNICALL
Java_org_lunetile_probe_ProbeActivity_runRubyProbe(JNIEnv *env, jclass klass) {
    (void)klass;
    static atomic_flag attempted = ATOMIC_FLAG_INIT;
    if (atomic_flag_test_and_set(&attempted)) {
        return (*env)->NewStringUTF(env, "Ruby probe already ran in this process");
    }
    int argc = 1;
    char name[] = "ruby-probe";
    char *args[] = {name, NULL};
    char **argv = args;
    ruby_sysinit(&argc, &argv);
    RUBY_INIT_STACK;
    if (ruby_setup() != 0) {
        return (*env)->NewStringUTF(env, "FAIL: ruby_setup");
    }
    if (ruby_probe_init_builtins()) {
        ruby_cleanup(1);
        return (*env)->NewStringUTF(env, "FAIL: Ruby builtin initialization");
    }
    int state = 0;
    VALUE value = rb_eval_string_protect("1 + 2", &state);
    int passed = !state && RB_INTEGER_TYPE_P(value) && NUM2INT(value) == 3;
    int cleanup = ruby_cleanup(passed ? 0 : 1);
    return (*env)->NewStringUTF(env,
        passed && cleanup == 0 ? "PASS: Ruby 1 + 2 = 3" : "FAIL: Ruby evaluation or cleanup");
}

JNIEXPORT jstring JNICALL
Java_org_lunetile_probe_ProbeMain_runRubyProbe(JNIEnv *env, jclass klass) {
    return Java_org_lunetile_probe_ProbeActivity_runRubyProbe(env, klass);
}
