#include <ruby.h>
#include "rgss_data.h"
#include "rgss_data_builtin.h"
#include <zlib.h>

void init_rgss_types();
extern "C" void Init_stringio();
extern "C" void ruby_init_ext(const char*, void (*)(void));

namespace {
VALUE inflate_script(VALUE, VALUE input) {
    StringValue(input);
    if (RSTRING_LEN(input) > 128 * 1024 * 1024) rb_raise(rb_eRuntimeError, "Compressed script exceeds 128 MiB");
    uInt length = static_cast<uInt>(RSTRING_LEN(input));
    for (uLongf capacity = 4096; capacity <= 128 * 1024 * 1024; capacity *= 2) {
        VALUE output = rb_str_new(nullptr, capacity);
        z_stream stream{};
        stream.next_in = reinterpret_cast<Bytef*>(RSTRING_PTR(input));
        stream.avail_in = length;
        stream.next_out = reinterpret_cast<Bytef*>(RSTRING_PTR(output));
        stream.avail_out = static_cast<uInt>(capacity);
        int status = inflateInit(&stream);
        if (status != Z_OK) rb_raise(rb_eRuntimeError, "Cannot initialize zlib: %s", zError(status));
        status = inflate(&stream, Z_FINISH);
        uLong actual = stream.total_out;
        uInt remaining = stream.avail_in;
        uInt available = stream.avail_out;
        inflateEnd(&stream);
        RB_GC_GUARD(input);
        if (status == Z_STREAM_END) {
            if (remaining != 0) rb_raise(rb_eRuntimeError, "Trailing bytes after zlib script");
            rb_str_set_len(output, actual);
            return output;
        }
        if (status != Z_BUF_ERROR || available != 0) rb_raise(rb_eRuntimeError, "Invalid zlib script: %s", zError(status));
    }
    rb_raise(rb_eRuntimeError, "Inflated script exceeds 128 MiB");
}
}

extern "C" VALUE lunetile_android_init_data(VALUE data_encoding) {
    VALUE module = rb_define_module("RGSSXP");
    StringValue(data_encoding);
    rb_define_const(module, "DATA_ENCODING", rb_obj_freeze(rb_str_dup(data_encoding)));
    rb_define_const(module, "RTP_PATHS", rb_obj_freeze(rb_ary_new()));
    init_rgss_types();
    rb_eval_string(value_types);
    rb_eval_string(data_types);
    rb_eval_string(text_support);
    rb_eval_string(integer_map_support);
    rb_define_singleton_method(module, "inflate", RUBY_METHOD_FUNC(inflate_script), 1);
    rb_eval_string(project_support);
    return rb_eval_string(script_support);
}

extern "C" VALUE lunetile_android_init_project(VALUE game_root) {
    VALUE module = rb_const_get(rb_cObject, rb_intern("RGSSXP"));
    StringValue(game_root);
    rb_define_const(module, "PROJECT_ROOT", rb_obj_freeze(rb_str_dup(game_root)));
    return Qnil;
}

extern "C" VALUE lunetile_android_init_saves(VALUE files_dir) {
    VALUE module = rb_const_get(rb_cObject, rb_intern("RGSSXP"));
    StringValue(files_dir);
    rb_define_const(module, "APP_FILES_DIR", rb_obj_freeze(rb_str_dup(files_dir)));
    ruby_init_ext("stringio.so", Init_stringio);
    rb_require("stringio");
    return rb_eval_string(save_support);
}
