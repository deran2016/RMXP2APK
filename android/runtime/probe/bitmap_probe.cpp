#include <jni.h>
#include <android/log.h>
#include <ruby.h>
#include <atomic>
#include <vector>
#include "../bitmap.h"
#include "../surface.h"
#include "../graphics.h"
#include "../ruby-probe-options.h"

extern "C" JNIEXPORT jstring JNICALL
Java_org_lunetile_probe_BitmapProbeActivity_runBitmapProbe(JNIEnv* env, jclass, jclass codec, jobject surface, jbyteArray png, jbyteArray graphics, jbyteArray tilemap) {
    static std::atomic_flag attempted = ATOMIC_FLAG_INIT;
    if (attempted.test_and_set()) return env->NewStringUTF("FAIL: bitmap probe already ran");
    if (!png || !graphics || !tilemap || !lunetile_bind_bitmap_codec(env, codec)) return env->NewStringUTF("FAIL: bitmap codec binding");
    jsize length = env->GetArrayLength(png);
    std::vector<char> data(static_cast<size_t>(length));
    env->GetByteArrayRegion(png, 0, length, reinterpret_cast<jbyte*>(data.data()));
    if (env->ExceptionCheck()) { lunetile_unbind_bitmap_codec(env); return nullptr; }
    jsize graphics_length = env->GetArrayLength(graphics);
    std::vector<char> graphics_source(static_cast<size_t>(graphics_length) + 1, 0);
    env->GetByteArrayRegion(graphics, 0, graphics_length, reinterpret_cast<jbyte*>(graphics_source.data()));
    jsize tilemap_length = env->GetArrayLength(tilemap);
    std::vector<char> tilemap_source(static_cast<size_t>(tilemap_length) + 1, 0);
    env->GetByteArrayRegion(tilemap, 0, tilemap_length, reinterpret_cast<jbyte*>(tilemap_source.data()));
    if (env->ExceptionCheck()) { lunetile_unbind_bitmap_codec(env); return nullptr; }
    int argc = 1;
    char name[] = "bitmap-probe";
    char* args[] = {name, nullptr};
    char** argv = args;
    ruby_sysinit(&argc, &argv);
    RUBY_INIT_STACK;
    if (ruby_setup() != 0) { lunetile_unbind_bitmap_codec(env); return env->NewStringUTF("FAIL: ruby_setup"); }
    if (ruby_probe_init_builtins()) { ruby_cleanup(1); lunetile_unbind_bitmap_codec(env); return env->NewStringUTF("FAIL: Ruby builtins"); }
    lunetile_init_bitmap();
    LunetileSurface output;
    if (!output.bind(env, surface)) { ruby_cleanup(1); lunetile_unbind_bitmap_codec(env); return env->NewStringUTF("FAIL: Surface bind"); }
    lunetile_init_graphics(&output, 2, 2);
    rb_gv_set("$probe_png", rb_str_new(data.data(), length));
    const char* setup =
        "class Color; attr_reader :red, :green, :blue, :alpha; "
        "def initialize(r,g,b,a=255); set(r,g,b,a); end; def set(r,g,b,a=255); @red,@green,@blue,@alpha=r,g,b,a; end; end; "
        "class Tone; attr_reader :red,:green,:blue,:gray; def initialize(r=0,g=0,b=0,a=0); set(r,g,b,a); end; def set(r,g,b,a); @red,@green,@blue,@gray=r,g,b,a; end; end; "
        "class Rect; attr_reader :x,:y,:width,:height; def initialize(x=0,y=0,w=0,h=0); set(x,y,w,h); end; "
        "def set(x,y,w,h); @x,@y,@width,@height=x,y,w,h; end; end; "
        "class Table; def [](*); nil; end; def xsize; 0; end; def ysize; 0; end; def zsize; 0; end; end; "
        "module RPG; end; module RGSSXP; def self.text(s); s; end; def self.read_file(*); $probe_png; end; "
        "def self.__fast_sprite_emit?; false; end; def self.scene_commands; __legacy_scene_commands; end; end";
    int state = 0;
    rb_eval_string_protect(setup, &state);
    if (!state) rb_eval_string_protect(graphics_source.data(), &state);
    if (!state) rb_eval_string_protect(tilemap_source.data(), &state);
    if (!state) rb_eval_string_protect("class Tilemap; def layers; legacy_layers; end; end", &state);
    const char* script =
        "image=Bitmap.new('fixture.png'); raise 'dimensions' unless image.width == 2 && image.height == 2; "
        "raise 'decode' unless image.get_pixel(0,0).red == 255 && image.get_pixel(1,1).blue == 255; "
        "copy=Bitmap.new(2,2); copy.blt(0,0,image,image.rect); raise 'blit' unless copy.get_pixel(0,0).red == 255; "
        "copy.fill_rect(1,0,1,1,Color.new(0,255,0,255)); raise 'fill' unless copy.get_pixel(1,0).green == 255; "
        "label=Bitmap.new(64,32); raise 'text metrics' unless label.text_size(\"\\uAC00\").width > 0; "
        "label.draw_text(0,0,64,32,\"\\uAC00\"); raise 'text pixels' unless label.__pixel_bytes.bytes.each_slice(4).any? { |p| p[3] > 0 }; "
        "tilemap=Tilemap.new; viewport=Viewport.new(0,0,2,2); viewport.tone=Tone.new(-100,0,0,0); "
        "sprite=Sprite.new(viewport); sprite.bitmap=copy; Graphics.update; raise 'frame' unless Graphics.frame_count == 1; "
        "raise 'viewport tone' unless Graphics.__scene_pixel(0,0) == [155,0,0,255]; "
        "Graphics.freeze; copy.fill_rect(0,0,2,2,Color.new(0,0,255,255)); mask=Bitmap.new(2,2); "
        "mask.fill_rect(1,0,1,1,Color.new(255,255,255,255)); Graphics.__present(RGSSXP.scene_commands,0.5,mask,0.2); "
        "raise 'transition mask' unless Graphics.__output_pixel(0,0) == [0,0,255,255] && Graphics.__output_pixel(1,0) == [0,255,0,255]; "
        "sprite.ox=2; sprite.oy=2; sprite.angle=180; Graphics.update; "
        "raise 'sprite rotation' unless Graphics.__scene_pixel(0,0) == [0,0,255,255]; true";
    VALUE result = state ? Qnil : rb_eval_string_protect(script, &state);
    bool passed = !state && RTEST(result);
    if (state) {
        VALUE message = rb_obj_as_string(rb_errinfo());
        __android_log_print(ANDROID_LOG_ERROR, "LunetileBitmapProbe", "Ruby error: %s", StringValueCStr(message));
    }
    output.clear();
    int cleanup = ruby_cleanup(passed ? 0 : 1);
    lunetile_unbind_bitmap_codec(env);
    return env->NewStringUTF(passed && cleanup == 0 ? "PASS: PNG, text, viewport tone, transition mask, Bitmap, Sprite, Surface" : "FAIL: PNG, text, tone, transition, Bitmap, Sprite or Surface");
}
