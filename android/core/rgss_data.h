#pragma once
#include <ruby.h>

// Call through rb_protect after Ruby is initialized; pass the project encoding string.
extern "C" VALUE lunetile_android_init_data(VALUE data_encoding);
// Call through rb_protect with the verified app-private game root after data initialization.
extern "C" VALUE lunetile_android_init_project(VALUE game_root);
// Call through rb_protect with Activity.getFilesDir() after lunetile_android_init_data.
extern "C" VALUE lunetile_android_init_saves(VALUE files_dir);
