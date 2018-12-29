#include <SFML/Graphics.hpp>
#include <cmath>
#include <random>
#include <string>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <array>
#include <assert.h>

constexpr uint32_t WINDOW_WIDTH  = 800;
constexpr uint32_t WINDOW_HEIGHT = 600;
constexpr uint32_t CELL_WIDTH    = 10;
constexpr uint32_t CELL_HEIGHT   = 10;
constexpr uint32_t GRID_WIDTH    = WINDOW_WIDTH/CELL_WIDTH;
constexpr uint32_t GRID_HEIGHT   = WINDOW_HEIGHT/CELL_HEIGHT;
constexpr uint32_t GRID_SIZE     = GRID_WIDTH*GRID_HEIGHT;
constexpr uint32_t PARTICLES     = 1000;
constexpr float    MAX_SPEED     = 2.0f;
constexpr float    MAX_GEN_SPEED = 0.01f;
constexpr uint32_t MUTATION_RATE = 4;
constexpr uint32_t CYCLE         = 1000;

static bool debug = false;
std::array<sf::Vertex, 4*PARTICLES> selection_probs;
size_t selection_probs_active_size = 0;
uint32_t generation = 0;

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

//Collision collisions[PARTICLES];
//uint32_t steps_to_goal[PARTICLES];
//sf::Vector2f positions[PARTICLES];
//sf::Vector2f velocities[PARTICLES];
//sf::Vector2f accelerations[PARTICLES];
sf::Vector2f fields[PARTICLES*GRID_SIZE];
sf::Vector2f old_fields[PARTICLES*GRID_SIZE];
float fitness[PARTICLES];

// Visualising
sf::Vertex      arrows[2*GRID_WIDTH*GRID_HEIGHT];

// Evo
float goal_radius = 20.0f;
sf::CircleShape goal_circle(goal_radius);
//sf::Vector2f goal = {0.9f*WINDOW_WIDTH, 0.5f*WINDOW_HEIGHT};
sf::Vector2f goal = {0.5f*WINDOW_WIDTH, 0.5f*WINDOW_HEIGHT};
sf::Vector2f start = {0.1f*WINDOW_WIDTH, 0.5f*WINDOW_HEIGHT};
constexpr uint32_t MAX_FITNESS = 100;

inline float norm(sf::Vector2f v){
	return sqrt(v.x*v.x+v.y*v.y);
}

inline float norm2(sf::Vector2f v){
	return v.x*v.x+v.y*v.y;
}

inline sf::Vector2f rotate(sf::Vector2f& v, float angle){
	float ca = cosf(angle);
	float sa = sinf(angle);
	return sf::Vector2f{ca*v.x - sa*v.y, sa*v.x + ca*v.y};
}

inline float dot(sf::Vector2f& a, sf::Vector2f& b){
	return a.x*b.x + a.y*b.y;
}

inline float angle_between(sf::Vector2f& a, sf::Vector2f& b){
	return atan2(b.y,b.x) - atan2(a.y,a.x);
}

inline sf::Vector2f rotate_to(sf::Vector2f& a, sf::Vector2f& b){
	auto angle = angle_between(a,b);
	return rotate(a, angle);
}

inline void init_particles_around_vec(sf::Vector2f vec){
	generation++;
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

	//return speed*sf::Vector2f{x,y};
	return sf::Vector2f{x,y};
}

void randomize_fields(){
	for(uint32_t i = 0; i < PARTICLES; i++){
		for(uint32_t x = 0; x < GRID_WIDTH; x++){
			for(uint32_t y = 0; y < GRID_HEIGHT; y++){
				fields[i*GRID_SIZE + x + y*GRID_WIDTH] = random_vector();
			}
		}
	}
}

int main(){
	// Test selection function
	{
		auto select = [&](auto data, float span){
			//std::random_device r;
			//std::mt19937 mt(r());

			std::uniform_real_distribution<float> select_dist(0,1);
			float r = select_dist(mt);
			int i = 0;
			while(r > 0.0f){
				r -= data[i++]/span;
			}
			return i-1;
			};


		constexpr uint32_t SAMPLE_SIZE = 10;
		uint32_t span = 0;
		std::array<int, SAMPLE_SIZE> data;
		for(uint32_t i = 0; i < SAMPLE_SIZE; i++){
			data[i] = i;
			span += i;
		}

		std::array<int, SAMPLE_SIZE> results;
		for(uint32_t i = 0; i < 1000; i++){ 
			auto j = select(data, span);
			results[j]++;
		}

		for(uint32_t i = 0; i < SAMPLE_SIZE; i++){
			printf("%i - %f : %f\n", i, static_cast<float>(data[i])/span, static_cast<float>(results[i])/1000.0f);
		}
	}

	std::string font_path = "../fonts/EBGaramond12-Regular.otf";
	sf::Font font;
	if(!font.loadFromFile(font_path)){
		printf("Unable to load font %s (file not found)!\n", font_path.c_str());
		return 1;
	}

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

	while(window.isOpen()){
		sf::Event event;
		while(window.pollEvent(event)){
			if(event.type == sf::Event::Closed)
					window.close();
			else if(event.type == sf::Event::KeyPressed){
				if(event.key.code == sf::Keyboard::Escape){
					window.close();
				}else if(event.key.code == sf::Keyboard::Space){
					debug = !debug;
				}
			}
		}

		window.clear(sf::Color::White);
		if(debug){
			auto float_rect = text.getLocalBounds();
			sf::Vector2f offset = {10,10};
			text_rect.setPosition(sf::Vector2f(float_rect.left-offset.x, float_rect.top-offset.y) + text.getPosition());
			text_rect.setSize({float_rect.width+2*offset.x, float_rect.height+2*offset.y});
			window.draw(arrows, 2*GRID_WIDTH*GRID_HEIGHT, sf::Lines);
		}
		window.draw(goal_circle);

		//
		// FITNESS EVALUATION
		//
		static uint32_t steps = 0;
		if(steps++ > CYCLE){
			steps = 0;

			float max_fitness = 0.0f;
			float total_fitness = 0.0f;
			for(uint32_t i = 0; i < PARTICLES; i++){
				auto& p = particles[i];
				auto dist = norm(p.position - goal);
				if(p.steps_to_goal <= 0)p.steps_to_goal = CYCLE;
				auto score = 1/(dist*p.steps_to_goal);
				score = pow(score, 6);
				if(p.hit_wall)score *= 0.01f;
				if(p.hit_goal)score *= 100.0f;

				fitness[i] = score;

				// track max fitness
				if(fitness[i] > max_fitness)max_fitness = fitness[i];
			}

			// Normalize
			total_fitness = 0.0f;
			max_fitness = (max_fitness > 0.0f) ? max_fitness : 1.0f;
			for(uint32_t i = 0; i < PARTICLES; i++){
				fitness[i] /= max_fitness;
				total_fitness += fitness[i];
			}

			if(debug){
				constexpr int WIDTH = 800;
				constexpr int HEIGHT = 50;
				selection_probs_active_size = 0;
				sf::Vector2f pos = {0, WINDOW_HEIGHT - HEIGHT};
				for(int i = 0; i < PARTICLES; i++){
					float f = fitness[i]/total_fitness;
					if (f > 0.0f){
						selection_probs[4*selection_probs_active_size+0] = sf::Vertex(pos, 													sf::Color::Red);
						selection_probs[4*selection_probs_active_size+1] = sf::Vertex(pos+sf::Vector2f{(float)WIDTH*f,0},  sf::Color::Blue);
						selection_probs[4*selection_probs_active_size+2] = sf::Vertex(pos+sf::Vector2f{(float)WIDTH*f,HEIGHT}, sf::Color::Blue);
						selection_probs[4*selection_probs_active_size+3] = sf::Vertex(pos+sf::Vector2f{0,HEIGHT}, 			sf::Color::Red);
						selection_probs_active_size++;
						pos.x += WIDTH*f;
					}
				}
			}

			auto select = [&](auto data){
				std::uniform_real_distribution<float> select_dist(0,1);
				float r = select_dist(mt);
				int i = 0;
				while(r > 0.0f){
					r -= data[i++]/total_fitness;
				}
				return i-1;
			};

			text.setString("Generation: " + std::to_string(generation) + "\n" + 
										 "Cycle: " + std::to_string(steps) + "/" + std::to_string(CYCLE) + "\n" + 
										 "Max fitness: " + std::to_string(1.0f/total_fitness) + "%\n" + 
										 "Total fitness: " + std::to_string(total_fitness) + "\n" +
										 "Mutation rate: " + std::to_string(MUTATION_RATE) + "%");

			// 
			// CROSSOVER + NEW POPULATION
			//
			memcpy(old_fields, fields, (PARTICLES*GRID_SIZE)*sizeof(sf::Vector2f));
			//std::normal_distribution<> split_dist(GRID_SIZE/2,GRID_SIZE/2);
			std::uniform_int_distribution<uint32_t> split_dist(0,GRID_SIZE);
			for(uint32_t i = 0; i < PARTICLES; i++){
				auto parent_1 = select(fitness);
				auto parent_2 = select(fitness);
				uint32_t split = std::round(split_dist(mt));

				//memcpy(fields+(i*GRID_SIZE), old_fields+(parent_1*GRID_SIZE), GRID_SIZE*sizeof(sf::Vector2f));
				memcpy(fields+(parent_1*GRID_SIZE), old_fields+(parent_1*GRID_SIZE), split*sizeof(sf::Vector2f));
				memcpy(fields+(parent_1*GRID_SIZE+split), old_fields+(parent_2*GRID_SIZE+split), (GRID_SIZE-split)*sizeof(sf::Vector2f));
			}

			//
			// MUTATION
			//
			std::uniform_int_distribution<uint32_t> coin(1,100);
			for(uint32_t i = 0; i < PARTICLES; i++){
				for(uint32_t x = 0; x < GRID_WIDTH; x++){
					for(uint32_t y = 0; y < GRID_HEIGHT; y++){
						if(coin(mt) <= MUTATION_RATE){
							fields[i*GRID_SIZE + x + y*GRID_WIDTH] = random_vector();
						}
					}
				}
			}

			//
			// DRAW LAST FIELD
			//
			if(debug){
				for(uint32_t x = 0; x < GRID_WIDTH; x++){
					for(uint32_t y = 0; y < GRID_HEIGHT; y++){
						auto vec = fields[(PARTICLES-1)*GRID_SIZE + x + y*GRID_WIDTH];
						auto speed = norm(vec);
						auto unit = vec/speed;
						//sf::Color color(speed*255.0f/MAX_SPEED,0,255.0f/MAX_SPEED/speed);
						sf::Color col1(255,0,0);
						sf::Color col2(0,0,255);
						arrows[2*(x+y*GRID_WIDTH)+0] = sf::Vertex({
								(CELL_WIDTH) *static_cast<float>(x) + CELL_WIDTH/2.0f  - 0.5f*(CELL_WIDTH/2.0f)*unit.x,
								(CELL_HEIGHT)*static_cast<float>(y) + CELL_HEIGHT/2.0f - 0.5f*(CELL_HEIGHT/2.0f)*unit.y,
						}, col1);
						arrows[2*(x+y*GRID_WIDTH)+1] = sf::Vertex({
								(CELL_WIDTH) *static_cast<float>(x) + CELL_WIDTH/2.0f  + 0.5f*(CELL_WIDTH/2.0f)*unit.x,
								(CELL_HEIGHT)*static_cast<float>(y) + CELL_HEIGHT/2.0f + 0.5f*(CELL_HEIGHT/2.0f)*unit.y,
						}, col2);
					}
				}
			}

			init_particles_around_vec(start);
		}

		for(uint32_t i = 0; i < PARTICLES; i++){
			auto& p = particles[i];
			
			if(p.position.x <= 0 || p.position.x >= WINDOW_WIDTH || p.position.y <= 0 || p.position.y >= WINDOW_HEIGHT){
				p.hit_wall = true;
			}
			if(norm(p.position - goal) <= goal_radius){
				p.hit_wall = true;
				p.steps_to_goal = CYCLE - steps;
			}

			if(!p.hit_wall && !p.hit_goal){
				uint32_t x = static_cast<uint32_t>(GRID_WIDTH*p.position.x/WINDOW_WIDTH);
				uint32_t y = static_cast<uint32_t>(GRID_HEIGHT*p.position.y/WINDOW_HEIGHT);
				auto vec = fields[i*GRID_SIZE + x + y*GRID_WIDTH];

				p.acceleration += 0.1f*vec;
				p.velocity += p.acceleration;

				float l = norm(p.velocity);
				if(l > MAX_SPEED){
					p.velocity *= (MAX_SPEED/l);
				};

				p.position += p.velocity;
				//if(isnan(position.x) || isnan(position.y))
				//	printf("p: %f,%f v: %f, %f a: %f,%f vec: %f, %f\n", position.x, position.y, velocity.x, velocity.y, acceleration.x, acceleration.y, vec.x, vec.y);
				p.acceleration = {0.0f,0.0f};
			}

			static sf::Vector2f x_axis = {1,0};
			static sf::Vector2f front = sf::Vector2f(3/sqrt(2), 0);
			static sf::Vector2f right = sf::Vector2f(0, 0.5f);
			auto angle = angle_between(front, p.velocity);
			auto rotated_front = rotate(front, angle);
			auto rotated_right = rotate(right, angle);

			particle_dots[3*i + 0] = sf::Vertex(p.position + 5.0f*rotated_front, sf::Color::Red);
			particle_dots[3*i + 1] = sf::Vertex(p.position + 5.0f*rotated_right, sf::Color::Red);
			particle_dots[3*i + 2] = sf::Vertex(p.position - 5.0f*rotated_right, sf::Color::Red);
		}

		window.draw(&particle_dots[0], particle_dots.size(), sf::Triangles);

		if(debug){
			window.draw(text_rect);
			window.draw(text);
			window.draw(&selection_probs[0], 4*selection_probs_active_size, sf::Quads);
		}
		window.display();
	}

	return 0;
}
