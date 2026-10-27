#include "surface.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <cstring>

LunetileSurface::~LunetileSurface() { clear(); }
bool LunetileSurface::bind(JNIEnv* env, jobject surface) {
    if (!env || !surface) return false;
    ANativeWindow* next = ANativeWindow_fromSurface(env, surface);
    if (!next) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if (window_) ANativeWindow_release(window_);
    window_ = next;
    return true;
}
void LunetileSurface::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (window_) ANativeWindow_release(window_);
    window_ = nullptr;
}
bool LunetileSurface::present(const lunetile::Raster& image) {
    static_assert(sizeof(lunetile::Pixel) == 4, "RGBA pixels must be packed");
    std::lock_guard<std::mutex> lock(mutex_);
    if (!window_ || ANativeWindow_setBuffersGeometry(window_, image.width(), image.height(), WINDOW_FORMAT_RGBA_8888) != 0) return false;
    ANativeWindow_Buffer buffer{};
    if (ANativeWindow_lock(window_, &buffer, nullptr) != 0) return false;
    bool valid = buffer.bits && buffer.format == WINDOW_FORMAT_RGBA_8888 && buffer.width == image.width() && buffer.height == image.height() && buffer.stride >= buffer.width;
    if (valid) {
        auto* destination = static_cast<lunetile::Pixel*>(buffer.bits);
        for (int y = 0; y < image.height(); ++y)
            std::memcpy(destination + size_t(y) * buffer.stride, image.data() + size_t(y) * image.width(), size_t(image.width()) * sizeof(lunetile::Pixel));
    }
    return ANativeWindow_unlockAndPost(window_) == 0 && valid;
}
