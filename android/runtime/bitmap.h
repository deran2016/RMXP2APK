#pragma once
#include <jni.h>
#include <ruby.h>
#include "raster.h"

/* Bind from a Java-owned thread before the Ruby game thread starts. */
bool lunetile_bind_bitmap_codec(JNIEnv* env, jclass codec);
void lunetile_unbind_bitmap_codec(JNIEnv* env);
void lunetile_init_bitmap();
const lunetile::Raster* lunetile_bitmap_raster(VALUE bitmap);
