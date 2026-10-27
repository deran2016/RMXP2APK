#pragma once
#include <jni.h>
int lunetile_input_register(JNIEnv *env, jobject state, int scale);
void lunetile_input_set_scale(int scale);
void lunetile_input_unregister(JNIEnv *env);
