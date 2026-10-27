#pragma once
#include <cstdint>
#include <vector>

namespace lunetile {
struct Pixel { uint8_t r, g, b, a; };
double transition_factor(double phase, double mask_red, double vague);
struct QuadEffect {
    double tone_red = 0, tone_green = 0, tone_blue = 0, tone_gray = 0;
    Pixel color{0, 0, 0, 0};
    int opacity = 255, bush_depth = 0, blend = 0;
    bool mirror = false;
};

class Raster {
public:
    Raster(int width, int height);
    int width() const { return width_; }
    int height() const { return height_; }
    Pixel* data() { return pixels_.data(); }
    const Pixel* data() const { return pixels_.data(); }
    const Pixel& at(int x, int y) const;
    void fill(int x, int y, int width, int height, Pixel color);
    void overlay(int x, int y, int width, int height, Pixel color);
    void tone(int x, int y, int width, int height, double red, double green, double blue, double gray);
    void blit(int x, int y, int width, int height, const Raster& source,
              int sx, int sy, int sw, int sh, uint8_t opacity = 255, bool mirror = false);
    void blit_clipped(int x, int y, int width, int height, const Raster& source,
                      int sx, int sy, int sw, int sh, uint8_t opacity, bool mirror,
                      int clip_x, int clip_y, int clip_width, int clip_height);
    void quad(double x, double y, double width, double height, double zoom_x, double zoom_y,
              double angle, double origin_x, double origin_y, const Raster& source,
              int sx, int sy, int sw, int sh, const QuadEffect& effect,
              int clip_x, int clip_y, int clip_width, int clip_height);
private:
    int width_, height_;
    std::vector<Pixel> pixels_;
};
}
