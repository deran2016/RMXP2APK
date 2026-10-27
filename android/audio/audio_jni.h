#pragma once
#include <jni.h>
int lunetile_audio_register(JNIEnv *env, jobject bridge);
void lunetile_audio_unregister(JNIEnv *env);
