#pragma once

#include <stdint.h>

namespace perlin{

void seed(uint32_t seed);
double noise(double x, double y, double z);


}
