#include "raster.h"
#include <cassert>
#include <climits>
#include <cstdio>
#include <stdexcept>
using lunetile::Raster;

int main() {
    assert(lunetile::transition_factor(0.5, 0, 0.2) == 1.0);
    assert(lunetile::transition_factor(0.5, 255, 0.2) == 0.0);
    assert(lunetile::transition_factor(0.5, 255, -1) == 0.5);
    try { Raster bad(INT_MAX, INT_MAX); assert(false); } catch (const std::invalid_argument&) {}
    Raster source(2, 1), target(4, 2);
    source.fill(0, 0, 1, 1, {255, 0, 0, 255});
    source.fill(1, 0, 1, 1, {0, 0, 255, 128});
    target.fill(0, 0, 4, 2, {0, 255, 0, 255});
    target.blit(-1, 0, 4, 1, source, 0, 0, 2, 1);
    assert(target.at(0, 0).r == 255 && target.at(0, 0).g == 0);
    assert(target.at(1, 0).b == 128 && target.at(1, 0).g == 127);
    assert(target.at(3, 0).g == 255);
    target.blit(0, 1, 2, 1, source, 0, 0, 2, 1, 255, true);
    assert(target.at(0, 1).b == 128 && target.at(1, 1).r == 255);
    source.blit(1, 0, 1, 1, source, 0, 0, 1, 1);
    assert(source.at(1, 0).r == 255 && source.at(1, 0).b == 0);
    target.fill(0, 0, 4, 2, {0, 0, 0, 255});
    target.blit_clipped(0, 0, 4, 1, source, 0, 0, 2, 1, 255, false, 1, 0, 1, 1);
    assert(target.at(0, 0).r == 0 && target.at(1, 0).r == 255 && target.at(2, 0).r == 0);
    target.fill(0, 0, 1, 1, {100, 150, 200, 255});
    target.tone(0, 0, 1, 1, 10, -10, 0, 0);
    assert(target.at(0, 0).r == 110 && target.at(0, 0).g == 140 && target.at(0, 0).b == 200);
    target.tone(0, 0, 1, 1, 0, 0, 0, 255);
    assert(target.at(0, 0).r == target.at(0, 0).g && target.at(0, 0).g == target.at(0, 0).b);
    target.overlay(0, 0, 1, 1, {255, 255, 255, 255});
    assert(target.at(0, 0).r == 255 && target.at(0, 0).g == 255 && target.at(0, 0).b == 255);
    Raster rotated_source(2, 1), rotated(3, 3);
    rotated_source.fill(0, 0, 1, 1, {255, 0, 0, 255});
    rotated_source.fill(1, 0, 1, 1, {0, 0, 255, 255});
    lunetile::QuadEffect effect;
    rotated.quad(2, 0, 2, 1, 1, 1, -90, 0, 0, rotated_source, 0, 0, 2, 1, effect, 0, 0, 3, 3);
    assert(rotated.at(1, 0).r == 255 && rotated.at(1, 1).b == 255);
    Raster effect_target(1, 1);
    effect_target.fill(0, 0, 1, 1, {10, 20, 30, 255});
    effect.tone_red = -100; effect.blend = 1; effect.opacity = 128;
    effect_target.quad(0, 0, 1, 1, 1, 1, 0, 0, 0, rotated_source, 0, 0, 1, 1, effect, 0, 0, 1, 1);
    assert(effect_target.at(0, 0).r == 88 && effect_target.at(0, 0).g == 20);
    effect = {};
    effect.color = {0, 0, 255, 128};
    effect_target.fill(0, 0, 1, 1, {0, 0, 0, 255});
    effect_target.quad(0, 0, 1, 1, 1, 1, 0, 0, 0, rotated_source, 0, 0, 1, 1, effect, 0, 0, 1, 1);
    assert(effect_target.at(0, 0).r == 127 && effect_target.at(0, 0).b == 128);
    effect = {}; effect.blend = 2; effect.opacity = 128; effect.bush_depth = 1;
    effect_target.fill(0, 0, 1, 1, {100, 100, 100, 255});
    effect_target.quad(0, 0, 1, 1, 1, 1, 0, 0, 0, rotated_source, 0, 0, 1, 1, effect, 0, 0, 1, 1);
    assert(effect_target.at(0, 0).r == 36 && effect_target.at(0, 0).g == 100);
    puts("raster tests passed");
}
