#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <stdint.h>
#include "constants.hpp"

namespace evo{

struct Particle{
	bool hit_wall = false;
	bool hit_goal = false;
	uint32_t steps_to_goal = 0;
	sf::Vector2f position;
	sf::Vector2f velocity;
	sf::Vector2f acceleration;
};

struct Debug{
	bool active = false;

	size_t selection_probs_active_size = 0;
	std::array<sf::Vertex, 4*PARTICLES> selection_probs;

	uint32_t current_generation = 0;

	float    max_fitness = 0.0f;
	float    total_fitness = 0.0f;
	uint32_t max_fitness_index = 0;

	sf::Vertex arrows[2*GRID_WIDTH*GRID_HEIGHT];
};

void initialize(sf::Vector2f goal, sf::Vector2f g, float g_radius);
void evaluate_fitness();
void crossover();
void mutate();
void next_generation();

std::array<Particle, PARTICLES>& particle_data();

sf::Vector2f& vector_in_field(uint32_t particle_index, uint32_t x, uint32_t y);
float normalized_fitness_for_particle(uint32_t particle_index);
float mass_for_particle(uint32_t particle_index);

void toggle_collect_debug_data();
Debug& debug_data();


}
