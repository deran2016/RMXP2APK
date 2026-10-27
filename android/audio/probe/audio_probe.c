#include <jni.h>
#include <ruby.h>
#include <stdatomic.h>
#include <stdio.h>
#include "../../runtime/ruby-probe-options.h"
#include "../audio_jni.h"

JNIEXPORT jstring JNICALL Java_org_lunetile_audio_AudioRubyProbeActivity_runProbe(JNIEnv *env, jclass cls, jobject bridge) {
    (void)cls;
    static atomic_flag used = ATOMIC_FLAG_INIT;
    if (atomic_flag_test_and_set(&used)) return (*env)->NewStringUTF(env, "FAIL: probe already used");
    int argc = 1; char name[] = "audio-probe"; char *args[] = {name, NULL}; char **argv = args;
    ruby_sysinit(&argc, &argv);
    RUBY_INIT_STACK;
    if (ruby_setup() != 0) return (*env)->NewStringUTF(env, "FAIL: ruby_setup");
    if (ruby_probe_init_builtins() || !lunetile_audio_register(env, bridge)) {
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        ruby_cleanup(1);
        return (*env)->NewStringUTF(env, "FAIL: initialization");
    }
    const char *script =
        "Audio.bgm_play('tone', -1, 200); Audio.bgs_play('tone.ogg', 55, 100); "
        "Audio.me_play('tone.wav', 60, 75); Audio.se_play('tone.wav', 50, 150); "
        "[:bgm, :bgs, :me, :se].each { |c| Audio.send(\"#{c}_stop\") }; "
        "['missing.wav','tone.mp3','../tone.wav','corrupt.ogg'].each do |path| "
        "  begin; Audio.se_play(path); rescue RuntimeError; next; end; raise \"not rejected: #{path}\"; "
        "end; begin; Audio.bgm_play('tone.wav', nil); raise 'accepted nil'; rescue TypeError; end; "
        "Audio.bgm_play('tone.wav'); Audio.bgs_play('tone.ogg'); Audio.me_play('tone.ogg'); "
        "Audio.bgm_fade(80); Audio.bgs_fade(80); Audio.me_fade(80); sleep 0.3; "
        "Audio.bgm_play('tone.mid'); Audio.bgm_stop; true";
    int state = 0;
    VALUE result = rb_eval_string_protect(script, &state);
    char message[512] = "PASS: Ruby Audio JNI WAV/OGG/MIDI and rejection checks";
    if (state || result != Qtrue) {
        VALUE exception = rb_errinfo();
        VALUE text = NIL_P(exception) ? Qnil : rb_obj_as_string(exception);
        snprintf(message, sizeof(message), "FAIL: %s", NIL_P(text) ? "Ruby evaluation" : StringValueCStr(text));
        rb_set_errinfo(Qnil);
    }
    lunetile_audio_unregister(env);
    ruby_cleanup(state ? 1 : 0);
    return (*env)->NewStringUTF(env, message);
}
