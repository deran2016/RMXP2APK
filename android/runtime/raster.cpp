#include "raster.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lunetile {
double transition_factor(double phase, double mask_red, double vague) {
    if (vague < 0) return std::clamp(phase, 0.0, 1.0);
    return std::clamp((phase * (1 + vague) - mask_red / 255.0) / std::max(vague, 0.0001), 0.0, 1.0);
}
namespace {
size_t count_pixels(int width, int height) {
    constexpr uint64_t max_pixels = 128ULL * 1024 * 1024 / sizeof(Pixel);
    if (width <= 0 || height <= 0 || uint64_t(width) * uint64_t(height) > max_pixels)
        throw std::invalid_argument("invalid raster dimensions");
    return size_t(width) * size_t(height);
}
Pixel blend(Pixel dst, Pixel src, uint8_t opacity) {
    unsigned sa = (unsigned(src.a) * opacity + 127) / 255;
    unsigned weight = sa * 255 + unsigned(dst.a) * (255 - sa);
    if (!weight) return {0, 0, 0, 0};
    auto channel = [&](unsigned d, unsigned s) -> uint8_t {
        return uint8_t((s * sa * 255 + d * unsigned(dst.a) * (255 - sa) + weight / 2) / weight);
    };
    return {channel(dst.r, src.r), channel(dst.g, src.g), channel(dst.b, src.b), uint8_t((weight + 127) / 255)};
}
}
Raster::Raster(int width, int height) : width_(width), height_(height), pixels_(count_pixels(width, height)) {}
const Pixel& Raster::at(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) throw std::out_of_range("pixel");
    return pixels_[size_t(y) * width_ + x];
}
void Raster::fill(int x, int y, int width, int height, Pixel color) {
    if (width <= 0 || height <= 0) return;
    int64_t x0 = std::max<int64_t>(0, x), y0 = std::max<int64_t>(0, y);
    int64_t x1 = std::min<int64_t>(width_, int64_t(x) + width), y1 = std::min<int64_t>(height_, int64_t(y) + height);
    if (x1 <= x0 || y1 <= y0) return;
    for (int64_t row = y0; row < y1; ++row) {
        auto begin = pixels_.begin() + size_t(row) * width_ + size_t(x0);
        std::fill(begin, begin + (x1 - x0), color);
    }
}
void Raster::overlay(int x, int y, int width, int height, Pixel color) {
    if (width <= 0 || height <= 0 || !color.a) return;
    int64_t x0 = std::max<int64_t>(0, x), y0 = std::max<int64_t>(0, y);
    int64_t x1 = std::min<int64_t>(width_, int64_t(x) + width), y1 = std::min<int64_t>(height_, int64_t(y) + height);
    for (int64_t row = y0; row < y1; ++row) for (int64_t col = x0; col < x1; ++col) {
        Pixel& destination = pixels_[size_t(row) * width_ + size_t(col)];
        destination = blend(destination, color, 255);
    }
}
void Raster::tone(int x, int y, int width, int height, double red, double green, double blue, double gray) {
    if (width <= 0 || height <= 0) return;
    gray = std::clamp(gray / 255.0, 0.0, 1.0);
    int64_t x0 = std::max<int64_t>(0, x), y0 = std::max<int64_t>(0, y);
    int64_t x1 = std::min<int64_t>(width_, int64_t(x) + width), y1 = std::min<int64_t>(height_, int64_t(y) + height);
    auto channel = [](double n) { return uint8_t(std::lround(std::clamp(n, 0.0, 255.0))); };
    for (int64_t row = y0; row < y1; ++row) for (int64_t col = x0; col < x1; ++col) {
        Pixel& p = pixels_[size_t(row) * width_ + size_t(col)];
        if (!p.a) continue;
        double luminance = p.r * 0.299 + p.g * 0.587 + p.b * 0.114;
        p = {channel(p.r * (1-gray) + luminance * gray + red), channel(p.g * (1-gray) + luminance * gray + green),
             channel(p.b * (1-gray) + luminance * gray + blue), p.a};
    }
}
void Raster::blit(int x, int y, int width, int height, const Raster& original,
                  int sx, int sy, int sw, int sh, uint8_t opacity, bool mirror) {
    blit_clipped(x, y, width, height, original, sx, sy, sw, sh, opacity, mirror, 0, 0, width_, height_);
}
void Raster::blit_clipped(int x, int y, int width, int height, const Raster& original,
                  int sx, int sy, int sw, int sh, uint8_t opacity, bool mirror,
                  int clip_x, int clip_y, int clip_width, int clip_height) {
    if (width <= 0 || height <= 0 || sw <= 0 || sh <= 0 || !opacity) return;
    if (clip_width <= 0 || clip_height <= 0) return;
    if (this == &original) {
        Raster copy(original);
        return blit_clipped(x, y, width, height, copy, sx, sy, sw, sh, opacity, mirror, clip_x, clip_y, clip_width, clip_height);
    }
    int64_t x0 = std::max<int64_t>({0, x, clip_x}), y0 = std::max<int64_t>({0, y, clip_y});
    int64_t x1 = std::min<int64_t>({width_, int64_t(x) + width, int64_t(clip_x) + clip_width});
    int64_t y1 = std::min<int64_t>({height_, int64_t(y) + height, int64_t(clip_y) + clip_height});
    for (int64_t row = y0; row < y1; ++row) {
        int64_t source_y = int64_t(sy) + (row - y) * sh / height;
        if (source_y < 0 || source_y >= original.height_) continue;
        for (int64_t col = x0; col < x1; ++col) {
            int64_t offset = (col - x) * sw / width;
            int64_t source_x = int64_t(sx) + (mirror ? sw - 1 - offset : offset);
            if (source_x < 0 || source_x >= original.width_) continue;
            Pixel& dst = pixels_[size_t(row) * width_ + size_t(col)];
            Pixel src = original.pixels_[size_t(source_y) * original.width_ + size_t(source_x)];
            if (!src.a) continue;
            dst = opacity == 255 && src.a == 255 ? src : blend(dst, src, opacity);
        }
    }
}
void Raster::quad(double x, double y, double width, double height, double zoom_x, double zoom_y,
                  double angle, double origin_x, double origin_y, const Raster& original,
                  int sx, int sy, int sw, int sh, const QuadEffect& effect,
                  int clip_x, int clip_y, int clip_width, int clip_height) {
    if (width <= 0 || height <= 0 || zoom_x == 0 || zoom_y == 0 || sw <= 0 || sh <= 0 ||
        clip_width <= 0 || clip_height <= 0 || effect.opacity <= 0) return;
    if (effect.blend < 0 || effect.blend > 2) throw std::invalid_argument("invalid blend mode");
    if (this == &original) {
        Raster copy(original);
        return quad(x, y, width, height, zoom_x, zoom_y, angle, origin_x, origin_y, copy,
                    sx, sy, sw, sh, effect, clip_x, clip_y, clip_width, clip_height);
    }
    const double radians = -angle * 3.14159265358979323846 / 180.0;
    const double c = std::cos(radians), s = std::sin(radians);
    double left = INFINITY, top = INFINITY, right = -INFINITY, bottom = -INFINITY;
    for (int corner = 0; corner < 4; ++corner) {
        double px = ((corner & 1 ? width : 0) - origin_x) * zoom_x;
        double py = ((corner & 2 ? height : 0) - origin_y) * zoom_y;
        double dx = x + px * c - py * s, dy = y + px * s + py * c;
        if (!std::isfinite(dx) || !std::isfinite(dy)) throw std::invalid_argument("invalid quad geometry");
        left = std::min(left, dx); right = std::max(right, dx); top = std::min(top, dy); bottom = std::max(bottom, dy);
    }
    left = std::max({std::floor(left), 0.0, double(clip_x)});
    top = std::max({std::floor(top), 0.0, double(clip_y)});
    right = std::min({std::ceil(right), double(width_), double(int64_t(clip_x) + clip_width)});
    bottom = std::min({std::ceil(bottom), double(height_), double(int64_t(clip_y) + clip_height)});
    if (left >= right || top >= bottom) return;
    const double gray = std::clamp(effect.tone_gray / 255.0, 0.0, 1.0), tint = effect.color.a / 255.0;
    auto channel = [](double value) { return uint8_t(std::lround(std::clamp(value, 0.0, 255.0))); };
    for (int row = int(top); row < int(bottom); ++row) for (int col = int(left); col < int(right); ++col) {
        double dx = col + 0.5 - x, dy = row + 0.5 - y;
        double lx = (dx * c + dy * s) / zoom_x + origin_x;
        double ly = (-dx * s + dy * c) / zoom_y + origin_y;
        if (lx < 0 || ly < 0 || lx >= width || ly >= height) continue;
        int64_t tx = int64_t(sx) + (effect.mirror ? sw - 1 - int64_t(lx / width * sw) : int64_t(lx / width * sw));
        int64_t ty = int64_t(sy) + int64_t(ly / height * sh);
        if (tx < 0 || ty < 0 || tx >= original.width_ || ty >= original.height_) continue;
        Pixel source = original.pixels_[size_t(ty) * original.width_ + size_t(tx)];
        if (!source.a) continue;
        double luminance = source.r * 0.299 + source.g * 0.587 + source.b * 0.114;
        auto effect_channel = [&](double value, double tone, int color) {
            double toned = std::clamp(value * (1 - gray) + luminance * gray + tone, 0.0, 255.0);
            return channel(toned * (1 - tint) + color * tint);
        };
        source.r = effect_channel(source.r, effect.tone_red, effect.color.r);
        source.g = effect_channel(source.g, effect.tone_green, effect.color.g);
        source.b = effect_channel(source.b, effect.tone_blue, effect.color.b);
        int opacity = std::clamp(effect.opacity, 0, 255);
        if (effect.bush_depth > 0 && ly >= height - effect.bush_depth) opacity = (opacity + 1) / 2;
        Pixel& destination = pixels_[size_t(row) * width_ + size_t(col)];
        if (!effect.blend) destination = blend(destination, source, uint8_t(opacity));
        else {
            double alpha = source.a * opacity / 65025.0;
            int sign = effect.blend == 1 ? 1 : -1;
            destination.r = channel(destination.r + sign * source.r * alpha);
            destination.g = channel(destination.g + sign * source.g * alpha);
            destination.b = channel(destination.b + sign * source.b * alpha);
            destination.a = blend(destination, source, uint8_t(opacity)).a;
        }
    }
}
}
