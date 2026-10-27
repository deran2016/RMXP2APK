#pragma once
#include "surface.h"

/* Surface must outlive the Ruby graphics module and be changed only while its game thread is stopped. */
void lunetile_init_graphics(LunetileSurface* surface, int width, int height, bool high_refresh = false);
void lunetile_graphics_set_wait_handler(bool (*handler)());
