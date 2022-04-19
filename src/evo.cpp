#include "evo.hpp"
#include "perlin.hpp"
#include "math_helper.hpp"
#include <math.h>
#include <random>
#include <string.h>
#include <assert.h>

namespace evo{
namespace /* data */ {
std::array<Particle, PARTICLES> particles;

std::array<sf::Vector2f, PARTICLES*GRID_SIZE> fields;
std::array<sf::Vector2f, PARTICLES*GRID_SIZE> old_fields;

std::array<float, PARTICLES> fitness;
std::array<float, PARTICLES> masses;

sf::Vector2f starting_point;
sf::Vector2f goal;
float goal_radius;

Debug debug;

float    max_fitness = 0.0f;
float    total_fitness = 0.0f;
uint32_t max_fitness_index = 0;

std::random_device r;
std::mt19937 mt(r());

inline sf::Vector2f random_vector(){
    static std::uniform_real_distribution<float> speed_dist(1.0f, MAX_GEN_SPEED);
    static std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f*3.1415f);

    float speed = speed_dist(mt);
    float angle = angle_dist(mt);
    float x = cosf(angle);
    float y = sinf(angle);

    return speed*sf::Vector2f{x,y};
}
}

void initialize(sf::Vector2f start, sf::Vector2f g, float g_radius){
    starting_point = start;
    goal = g;
    goal_radius = g_radius;
    //randomize_fields();
    next_generation();

    // randomize initial conditions
    std::uniform_int_distribution<int32_t> seed_dist(std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max());
    std::uniform_real_distribution<float> speed_dist(0.1f, MAX_GEN_SPEED);
    std::uniform_real_distribution<float> mass_dist(0.01f, 1.0f);
    for(uint32_t i = 0; i < PARTICLES; i++){
        masses[i] = mass_dist(mt);
        perlin::seed(seed_dist(mt));
        for(uint32_t x = 0; x < GRID_WIDTH; x++){
            for(uint32_t y = 0; y < GRID_HEIGHT; y++){
                fields[i*GRID_SIZE + x + y*GRID_WIDTH] = random_vector();

                //float angle = 2*M_PI*perlin::noise(
                //	static_cast<float>(x)/static_cast<float>(GRID_WIDTH),
                //	static_cast<float>(y)/static_cast<float>(GRID_HEIGHT),
                //	static_cast<float>(x+y+i)/static_cast<float>(PARTICLES+GRID_WIDTH+GRID_HEIGHT)
                //	);

                //fields[i*GRID_SIZE + x + y*GRID_WIDTH] = speed_dist(mt)*sf::Vector2f(cosf(angle),sinf(angle));
            }
        }
    }
}

float fitness_function(Particle &p) {
    auto dist = fmax(norm(p.position - goal), goal_radius);
    assert(p.steps_to_goal > 0);
    float score = 1.0 + WINDOW_WIDTH - dist;
    score += 1.0f/(dist*pow((float)p.steps_to_goal/CYCLE, 5));
    if(p.hit_wall)
        score *= 1e-2;
    if(p.hit_goal)
        score *= 1e2;
    return score;
}

void evaluate_fitness(){
    total_fitness = 0.0f;
    max_fitness = 0.0f;
    max_fitness_index = 0;

    for(uint32_t i = 0; i < PARTICLES; i++){
        auto& p = particles[i];

        auto score = fitness_function(p);

        fitness[i] = score;

        // track max fitness
        if(fitness[i] > max_fitness){
            max_fitness = fitness[i];
            max_fitness_index = i;
        }
    }

    // Normalize
    max_fitness = (max_fitness > 0.0f) ? max_fitness : 1.0f;
    for(uint32_t i = 0; i < PARTICLES; i++){
        fitness[i] /= max_fitness;
        total_fitness += fitness[i];
    }

    if (debug.active) {
        debug.total_fitness = total_fitness;
        debug.max_fitness = max_fitness;
        debug.max_fitness_index = max_fitness_index;
    }
}

void crossover(){
    static auto select = [&](auto data) -> uint32_t {
        if (total_fitness == 0.0f) {
            return 0;
        }

        std::uniform_real_distribution<float> select_dist(0,1);
        float r = select_dist(mt);

        for (uint32_t i = 0; i < data.size(); ++i) {
            r -= data[i]/total_fitness;
            if (r <= 0.0f)
                return i;
        }

        return 0;
    };

    memcpy(old_fields.data(), fields.data(), (PARTICLES*GRID_SIZE)*sizeof(sf::Vector2f));
    std::uniform_int_distribution<uint32_t> split_dist(0,GRID_SIZE);
    for(uint32_t i = 0; i < PARTICLES; i++){
        auto parent_1 = select(fitness);
        auto parent_2 = select(fitness);

        uint32_t split = std::round(split_dist(mt));

        memcpy(fields.data()+(i*GRID_SIZE),       old_fields.data()+(parent_1*GRID_SIZE), split*sizeof(sf::Vector2f));
        memcpy(fields.data()+(i*GRID_SIZE+split), old_fields.data()+(parent_2*GRID_SIZE+split), (GRID_SIZE-split)*sizeof(sf::Vector2f));
        masses[i] = (masses[parent_1] + masses[parent_2])/2.0f;
    }
}

void mutate(){
    std::uniform_int_distribution<uint32_t> gene_dist(0, GRID_SIZE*PARTICLES - 1);
    std::uniform_int_distribution<uint32_t> mass_index_dist(0, PARTICLES - 1);
    std::uniform_real_distribution<float> mass_dist(0.01f, 1.0f);
    for(uint32_t i = 0; i < MUTATIONS_TO_PERFORM; i++){
        fields[gene_dist(mt)] = random_vector();
        masses[mass_index_dist(mt)] = mass_dist(mt);
    }
}

void next_generation(){
    debug.current_generation++;
    for(auto& p : particles){
        p.hit_goal      = false;
        p.hit_wall      = false;
        p.position      = starting_point;
        p.velocity      = {0.0f, 0.0f};
        p.acceleration  = {0.0f, 0.0f};
        p.steps_to_goal = CYCLE;
    }
}

std::array<Particle, PARTICLES>& particle_data(){
    return particles;
}

sf::Vector2f& vector_in_field(uint32_t particle_index, uint32_t x, uint32_t y){
    return fields[particle_index*GRID_SIZE + x + y*GRID_WIDTH];
}

float normalized_fitness_for_particle(uint32_t particle_index){
    return fitness[particle_index]/debug.total_fitness;
}

float mass_for_particle(uint32_t particle_index){
    return masses[particle_index];
}

void toggle_collect_debug_data(){
    debug.active = !debug.active;
}

Debug& debug_data(){
    return debug;
}

}
