#pragma once
#include <jni.h>
#include <mutex>
#include "raster.h"

struct ANativeWindow;
class LunetileSurface {
public:
    ~LunetileSurface();
    bool bind(JNIEnv* env, jobject surface);
    void clear();
    bool present(const lunetile::Raster& image);
private:
    std::mutex mutex_;
    ANativeWindow* window_ = nullptr;
};
