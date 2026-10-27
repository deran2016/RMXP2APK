#include <jni.h>
#include <ruby.h>
#include <cstring>
#include "rgss_data.h"

extern "C" JNIEXPORT jstring JNICALL
Java_org_lunetile_savediagnostic20261027_ProbeActivity_run(JNIEnv* env, jclass, jstring directory, jstring mode) {
    int argc = 1;
    char program[] = "save-probe";
    char* arguments[] = {program, nullptr};
    char** argv = arguments;
    ruby_sysinit(&argc, &argv);
    RUBY_INIT_STACK;
    if (ruby_setup() != 0) return env->NewStringUTF("FAIL ruby_setup");
    char no_gems[] = "--disable-gems", expression[] = "-e", empty[] = "";
    char* options[] = {program, no_gems, expression, empty, nullptr};
    int state = 0;
    void* node = ruby_options(4, options);
    if (!ruby_executable_node(node, &state) || ruby_exec_node(node) != 0)
        return env->NewStringUTF("FAIL ruby_options");
    rb_protect(lunetile_android_init_data, rb_utf8_str_new_cstr("UTF-8"), &state);
    const char* path = env->GetStringUTFChars(directory, nullptr);
    if (!path) return env->NewStringUTF("FAIL filesDir");
    if (!state) rb_protect(lunetile_android_init_saves, rb_utf8_str_new_cstr(path), &state);
    env->ReleaseStringUTFChars(directory, path);
    const char* action = env->GetStringUTFChars(mode, nullptr);
    bool write = action && std::strcmp(action, "write") == 0;
    if (action) env->ReleaseStringUTFChars(mode, action);
    if (!state) rb_eval_string_protect(write ? R"RUBY(
      path = RGSSXP::Saves.filename(0, :write)
      RGSSXP::Saves.write(path) { |f| Marshal.dump({'map' => 1, 'value' => 1977}, f) }
      original = File.binread(path)
      begin
        RGSSXP::Saves.write(path) { |f| f.write('invalid') }
        raise 'invalid save accepted'
      rescue ArgumentError, TypeError, EOFError
      end
      raise 'failed write damaged save' unless File.binread(path) == original
      RGSSXP::Saves.write(path) { |f| Marshal.dump({'map' => 1, 'value' => 1978}, f) }
      raise 'backup missing' unless File.binread(path + '.bak1') == original
      high_path = RGSSXP::Saves.filename(1, :write)
      high = RGSSXP::Saves::HIGH_MAGIC + Marshal.dump(1)
      File.binwrite(high_path, high)
      begin
        RGSSXP::Saves.open_read(high_path)
        raise '120Hz save unexpectedly accepted'
      rescue IOError => error
        raise unless error.message.include?('120Hz')
      end
      raise '120Hz save changed' unless File.binread(high_path) == high
    )RUBY" : R"RUBY(
      path = RGSSXP::Saves.filename(0)
      raise 'slot missing' unless RGSSXP::Saves.exist?(0)
      stream = RGSSXP::Saves.open_read(path)
      value = Marshal.load(stream)
      stream.close
      raise 'wrong saved data' unless value == {'map' => 1, 'value' => 1978}
      old = File.binread(path + '.bak1')
      raise 'wrong backup' unless Marshal.load(old.byteslice(RGSSXP::Saves::MAGIC.bytesize..))['value'] == 1977
    )RUBY", &state);
    if (state) {
        VALUE message = rb_obj_as_string(rb_errinfo());
        jstring result = env->NewStringUTF(StringValueCStr(message));
        ruby_cleanup(1);
        return result;
    }
    int cleanup = ruby_cleanup(0);
    return env->NewStringUTF(cleanup ? "FAIL ruby_cleanup" : write ? "PASS write and rollback" : "PASS restart read and backup");
}
