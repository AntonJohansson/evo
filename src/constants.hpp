#pragma once

#include <stdint.h>

constexpr uint32_t WINDOW_WIDTH  = 800;
constexpr uint32_t WINDOW_HEIGHT = 600;
constexpr uint32_t CELL_WIDTH    = 10;
constexpr uint32_t CELL_HEIGHT   = 10;
constexpr uint32_t GRID_WIDTH    = WINDOW_WIDTH/CELL_WIDTH;
constexpr uint32_t GRID_HEIGHT   = WINDOW_HEIGHT/CELL_HEIGHT;
constexpr uint32_t GRID_SIZE     = GRID_WIDTH*GRID_HEIGHT;

constexpr uint32_t PARTICLES     = 1000;

constexpr float    MAX_SPEED     = 2.0f;
constexpr float    MAX_GEN_SPEED = 0.05f;

constexpr uint32_t CYCLE         = 2000;

constexpr float    MUTATION_RATE = 0.1f;
constexpr uint32_t MUTATIONS_TO_PERFORM = GRID_SIZE * PARTICLES * (MUTATION_RATE/100.0f);
