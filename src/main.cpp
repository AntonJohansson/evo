#include <SFML/Graphics.hpp>
#include <cmath>
#include <random>
#include <string>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <array>
#include <assert.h>

#include <sstream>
#include <iomanip>

#include "math_helper.hpp"
#include "profiling.hpp"

#include "perlin.hpp"
#include <limits>
#include <algorithm>

#include <vector>

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
constexpr uint32_t MUTATION_RATE = 4;
constexpr uint32_t CYCLE         = 2000;

std::vector<sf::RectangleShape> walls;
bool drawing_shape = false;
sf::Vector2i mouse_pos;
sf::Vector2f shape_anchor;
sf::Vector2f shape_corner;
sf::RectangleShape active_shape;

constexpr uint32_t HISTORY_SIZE = 10*PARTICLES;
uint32_t history_active_size = 0;
std::array<sf::Vertex, HISTORY_SIZE> history;

struct Debug{
	bool active = false;

	size_t selection_probs_active_size = 0;
	std::array<sf::Vertex, 4*PARTICLES> selection_probs;

	uint32_t generation = 0;

	uint32_t max_fitness_index = 0;
	sf::Vertex arrows[2*GRID_WIDTH*GRID_HEIGHT];
} debug;


std::random_device r;
std::mt19937 mt(r());

struct Particle{
	bool hit_wall = false;
	bool hit_goal = false;
	uint32_t steps_to_goal = 0;
	sf::Vector2f position;
	sf::Vector2f velocity;
	sf::Vector2f acceleration;
};

std::array<sf::Vertex, 3*PARTICLES> particle_dots;
std::array<Particle, PARTICLES> particles;

sf::Vector2f fields[PARTICLES*GRID_SIZE];
sf::Vector2f old_fields[PARTICLES*GRID_SIZE];
float fitness[PARTICLES];

// Evo
float goal_radius = 20.0f;
sf::CircleShape goal_circle(goal_radius);
sf::Vector2f goal = {0.9f*WINDOW_WIDTH, 0.5f*WINDOW_HEIGHT};
//sf::Vector2f goal = {0.5f*WINDOW_WIDTH, 0.5f*WINDOW_HEIGHT};
sf::Vector2f start = {0.1f*WINDOW_WIDTH, 0.5f*WINDOW_HEIGHT};


inline void init_particles_around_vec(sf::Vector2f vec){
	debug.generation++;
	for(auto& p : particles){
		p.hit_goal = false;
		p.hit_wall = false;
		p.position 			= vec;
		p.velocity 			= {0.0f, 0.0f};
		p.acceleration 	= {0.0f, 0.0f};
		p.steps_to_goal = 0;
	}
}

inline sf::Vector2f random_vector(){
	static std::uniform_real_distribution<float> speed_dist(1.0f, MAX_GEN_SPEED);
	static std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f*3.1415f);

	float speed = speed_dist(mt);
	float angle = angle_dist(mt);
	float x = cosf(angle);
	float y = sinf(angle);

	return speed*sf::Vector2f{x,y};
	//return sf::Vector2f{x,y};
}

void randomize_fields(){
	std::uniform_int_distribution<int32_t> seed_dist(std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max());
	std::uniform_real_distribution<float> speed_dist(1.0f, MAX_GEN_SPEED);
	for(uint32_t i = 0; i < PARTICLES; i++){
		perlin::seed(seed_dist(mt));
		for(uint32_t x = 0; x < GRID_WIDTH; x++){
			for(uint32_t y = 0; y < GRID_HEIGHT; y++){
				//fields[i*GRID_SIZE + x + y*GRID_WIDTH] = random_vector();
					
				float angle = 2*M_PI*perlin::noise(
					static_cast<float>(x)/static_cast<float>(GRID_WIDTH),
					static_cast<float>(y)/static_cast<float>(GRID_HEIGHT),
					static_cast<float>(x+y+i)/static_cast<float>(PARTICLES+GRID_WIDTH+GRID_HEIGHT)
					);

				fields[i*GRID_SIZE + x + y*GRID_WIDTH] = speed_dist(mt)*sf::Vector2f(cosf(angle),sinf(angle));
			}
		}
	}
}

int main(){
	const std::string font_path = "../fonts/Roboto-Regular.ttf";
	sf::Font font;
	if(!font.loadFromFile(font_path)){
		printf("Unable to load font %s (file not found)!\n", font_path.c_str());
		return 1;
	}

	std::stringstream ui_stream;
	sf::Text text;
	text.setFont(font);
	text.setCharacterSize(16);
	text.setPosition({50,25});
	text.setFillColor(sf::Color::White);
	text.setString("Max fitness: 0%");

	sf::RectangleShape text_rect;
	text_rect.setFillColor(sf::Color(0,0,0,200));

	sf::ContextSettings settings;
	settings.antialiasingLevel = 8;
	sf::RenderWindow window(sf::VideoMode(800,600), "evo", sf::Style::Close, settings);

	goal_circle.setFillColor(sf::Color::Green);
	goal_circle.setOrigin(sf::Vector2f{goal_radius, goal_radius});
	goal_circle.setPosition(goal);

	randomize_fields();
	init_particles_around_vec(start);

	//
	// LOOPY
	//
	
	while(window.isOpen()){
		sf::Event event;
		while(window.pollEvent(event)){
			if(event.type == sf::Event::Closed)
					window.close();
			else if(event.type == sf::Event::KeyPressed){
				if(event.key.code == sf::Keyboard::Escape){
					window.close();
				}else if(event.key.code == sf::Keyboard::Space){
					debug.active = !debug.active;
				}
			}else if(event.type == sf::Event::MouseButtonPressed){
				if(!drawing_shape){
					mouse_pos = sf::Mouse::getPosition(window);
					shape_anchor = sf::Vector2f(static_cast<float>(mouse_pos.x), static_cast<float>(mouse_pos.y));
					active_shape.setFillColor(sf::Color(255,100,100));
					drawing_shape = true;
				}else{
					active_shape.setFillColor(sf::Color(100,100,100));
					walls.push_back(active_shape);
					drawing_shape = false;
				}
			}
		}

		if(drawing_shape){
			mouse_pos = sf::Mouse::getPosition(window);
			shape_corner = sf::Vector2f(static_cast<float>(mouse_pos.x), static_cast<float>(mouse_pos.y));

			active_shape.setSize(shape_corner - shape_anchor);
			active_shape.setPosition(shape_anchor);
		}
		
		//
		// EVO
		//
		static uint32_t steps = 0;
		if(steps++ > CYCLE){
			PROFILE_SCOPE("evo")
			steps = 0;

			ui_stream.clear();
			ui_stream.str(std::string());

			float max_fitness = 0.0f;
			float total_fitness = 0.0f;
			{PROFILE_SCOPE("evo_fitness")
				for(uint32_t i = 0; i < PARTICLES; i++){
					auto& p = particles[i];
					auto dist = norm(p.position - goal);
					if(p.steps_to_goal <= 0)p.steps_to_goal = CYCLE;
					auto score = 1/(dist*p.steps_to_goal);
					score = pow(score, 6);
					if(p.hit_wall)score *= 0.01f;
					if(p.hit_goal)score *= 100000.0f;

					fitness[i] = score;

					// track max fitness
					if(fitness[i] > max_fitness){
						max_fitness = fitness[i];
						debug.max_fitness_index = i;
					}
				}

				// Normalize
				max_fitness = (max_fitness > 0.0f) ? max_fitness : 1.0f;
				for(uint32_t i = 0; i < PARTICLES; i++){
					fitness[i] /= max_fitness;
					total_fitness += fitness[i];
				}
			}

			{PROFILE_SCOPE("evo_crossover")
				static auto select = [&](auto data){
					std::uniform_real_distribution<float> select_dist(0,1);
					float r = select_dist(mt);
					int i = 0;
					while(r > 0.0f){
						r -= data[i++]/total_fitness;
					}
					return i-1;
				};

				memcpy(old_fields, fields, (PARTICLES*GRID_SIZE)*sizeof(sf::Vector2f));
				std::uniform_int_distribution<uint32_t> split_dist(0,GRID_SIZE);
				for(uint32_t i = 0; i < PARTICLES; i++){
					auto parent_1 = select(fitness);
					auto parent_2 = select(fitness);

					uint32_t split = std::round(split_dist(mt));

					//memcpy(fields+(i*GRID_SIZE), old_fields+(parent_1*GRID_SIZE), GRID_SIZE*sizeof(sf::Vector2f));
					memcpy(fields+(i*GRID_SIZE),       old_fields+(parent_1*GRID_SIZE), split*sizeof(sf::Vector2f));
					memcpy(fields+(i*GRID_SIZE+split), old_fields+(parent_2*GRID_SIZE+split), (GRID_SIZE-split)*sizeof(sf::Vector2f));
				}

			}

			uint32_t mutations_to_perform = GRID_SIZE * PARTICLES * (MUTATION_RATE/100.0f);
			{PROFILE_SCOPE("evo_mutation_new")
				std::uniform_int_distribution<uint32_t> gene_dist(0, GRID_SIZE*PARTICLES - 1);
				for(uint32_t i = 0; i < mutations_to_perform; i++){
					fields[gene_dist(mt)] = random_vector();
				}
			}
			
			if(debug.active){
				PROFILE_SCOPE("evo_debug")
				for(uint32_t x = 0; x < GRID_WIDTH; x++){
					for(uint32_t y = 0; y < GRID_HEIGHT; y++){
						auto vec = fields[debug.max_fitness_index*GRID_SIZE + x + y*GRID_WIDTH];
						auto speed = norm(vec);
						auto unit = vec/speed;
						//sf::Color color(speed*255.0f/MAX_SPEED,0,255.0f/MAX_SPEED/speed);
						sf::Color col1(255,0,0);
						sf::Color col2(0,0,255);
						debug.arrows[2*(x+y*GRID_WIDTH)+0] = sf::Vertex({
								(CELL_WIDTH) *static_cast<float>(x) + CELL_WIDTH/2.0f  - 0.5f*(CELL_WIDTH/2.0f)*unit.x,
								(CELL_HEIGHT)*static_cast<float>(y) + CELL_HEIGHT/2.0f - 0.5f*(CELL_HEIGHT/2.0f)*unit.y,
						}, col1);
						debug.arrows[2*(x+y*GRID_WIDTH)+1] = sf::Vertex({
								(CELL_WIDTH) *static_cast<float>(x) + CELL_WIDTH/2.0f  + 0.5f*(CELL_WIDTH/2.0f)*unit.x,
								(CELL_HEIGHT)*static_cast<float>(y) + CELL_HEIGHT/2.0f + 0.5f*(CELL_HEIGHT/2.0f)*unit.y,
						}, col2);
					}
				}

				constexpr int WIDTH = 800;
				constexpr int HEIGHT = 50;
				debug.selection_probs_active_size = 0;
				sf::Vector2f pos = {0, WINDOW_HEIGHT - HEIGHT};
				for(uint32_t i = 0; i < PARTICLES; i++){
					float f = fitness[i]/total_fitness;
					if (f > 0.0f){
						debug.selection_probs[4*debug.selection_probs_active_size+0] = sf::Vertex(pos, 													sf::Color::Red);
						debug.selection_probs[4*debug.selection_probs_active_size+1] = sf::Vertex(pos+sf::Vector2f{(float)WIDTH*f,0},  sf::Color::Blue);
						debug.selection_probs[4*debug.selection_probs_active_size+2] = sf::Vertex(pos+sf::Vector2f{(float)WIDTH*f,HEIGHT}, sf::Color::Blue);
						debug.selection_probs[4*debug.selection_probs_active_size+3] = sf::Vertex(pos+sf::Vector2f{0,HEIGHT}, 			sf::Color::Red);
						debug.selection_probs_active_size++;
						pos.x += WIDTH*f;
					}
				}

				ui_stream 
					<< "Generation:      "	<< debug.generation << "\n"
					<< "Cycle:           "	<< steps << "/" << CYCLE << "\n"
					<< "Max fitness:     "	<< 1.0f/total_fitness << "%\n"
					<< "Total fitness:   "	<< total_fitness << "\n"
					<< "Mutation rate:   "	<< MUTATION_RATE << "%\n"
					<< "Mutations (new): "	<< mutations_to_perform << "\n";

				ui_stream << "Profiling: \n";
				for(auto& [name, time] : profile::data){
					ui_stream << name << "  " << std::left << time << "\n";
				}
			}

			init_particles_around_vec(start);
		}

		//
		// UPDATE
		//
		PROFILE_SCOPE("update"){
			for(uint32_t i = 0; i < PARTICLES; i++){

				auto& p = particles[i];

				// Check collision

				if(p.position.x <= 0 || p.position.x >= WINDOW_WIDTH || p.position.y <= 0 || p.position.y >= WINDOW_HEIGHT){
					p.hit_wall = true;
				}
				if(norm(p.position - goal) <= goal_radius){
					p.hit_wall = true;
					p.steps_to_goal = CYCLE - steps;
				}

				for(auto& rect : walls){
					const auto& bounds = rect.getGlobalBounds();
					if(bounds.contains(p.position))
						p.hit_wall = true;
				}

				// update movement
				if(!p.hit_wall && !p.hit_goal){
					uint32_t x = static_cast<uint32_t>(GRID_WIDTH*p.position.x/WINDOW_WIDTH);
					uint32_t y = static_cast<uint32_t>(GRID_HEIGHT*p.position.y/WINDOW_HEIGHT);
					auto vec = fields[i*GRID_SIZE + x + y*GRID_WIDTH];

					//p.velocity += 0.1f*vec;
					p.velocity += 0.05f*vec;

					float l = norm(p.velocity);
					if(l > MAX_SPEED){
						p.velocity *= (MAX_SPEED/l);
					};

					history[history_active_size++%(HISTORY_SIZE)] = sf::Vertex(p.position, sf::Color(255,50,50,100));
					p.position += p.velocity;
					//if(isnan(position.x) || isnan(position.y))
					//	printf("p: %f,%f v: %f, %f a: %f,%f vec: %f, %f\n", position.x, position.y, velocity.x, velocity.y, acceleration.x, acceleration.y, vec.x, vec.y);
				}

				static sf::Vector2f x_axis = {1,0};
				static sf::Vector2f front = sf::Vector2f(3/sqrt(2), 0);
				static sf::Vector2f right = sf::Vector2f(0, 0.5f);
				auto angle = angle_between(front, p.velocity);
				auto rotated_front = rotate(front, angle);
				auto rotated_right = rotate(right, angle);

				particle_dots[3*i + 0] = sf::Vertex(p.position + 5.0f*rotated_front, sf::Color(50, 50, 50));
				particle_dots[3*i + 1] = sf::Vertex(p.position + 5.0f*rotated_right, sf::Color(255*norm(p.velocity)/MAX_SPEED, 50, 50));
				particle_dots[3*i + 2] = sf::Vertex(p.position - 5.0f*rotated_right, sf::Color(255*norm(p.velocity)/MAX_SPEED, 50, 50));
			}
		}

		{PROFILE_SCOPE("draw")
			window.clear(sf::Color::White);
			if(debug.active){
				auto float_rect = text.getLocalBounds();
				sf::Vector2f offset = {10,10};
				text_rect.setPosition(sf::Vector2f(float_rect.left-offset.x, float_rect.top-offset.y) + text.getPosition());
				text_rect.setSize({float_rect.width+2*offset.x, float_rect.height+2*offset.y});
				window.draw(debug.arrows, 2*GRID_WIDTH*GRID_HEIGHT, sf::Lines);
			}
			window.draw(goal_circle);

			for(auto& rect : walls){
				window.draw(rect);
			}
			if(drawing_shape){
				window.draw(active_shape);
			}

			window.draw(&history[0], (history_active_size > history.size()) ? history.size() : history_active_size, sf::Points);
			window.draw(&particle_dots[0], particle_dots.size(), sf::Triangles);

			if(debug.active){
				text.setString(ui_stream.str());
				window.draw(text_rect);
				window.draw(text);
				window.draw(&debug.selection_probs[0], 4*debug.selection_probs_active_size, sf::Quads);
			}
			window.display();
		}
	}

	return 0;
}
