#include <SFML/Graphics.hpp>
#include <cmath>
#include <random>
#include <string>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <array>
#include <assert.h>
#include <algorithm>

#include <sstream>
#include <iomanip>

#include "math_helper.hpp"
#include "profiling.hpp"

#include "perlin.hpp"
#include <limits>
#include <algorithm>

#include "evo.hpp"
#include "constants.hpp"

#include <vector>

// Settings

bool pause = false;
int32_t draw_every = 1;
int32_t draw_top = 10;
bool draw_trail = true;
bool show_profile = false;
bool show_fitness_percentage = false;
bool show_evo_metric = false;
bool show_best_vector_field = false;

int32_t particle_count = 1000;
float max_gen_speed = 0.05f;
float max_speed = 2.0f;
float mutation_rate = 0.1f;
int32_t steps_per_generation = 2000;

enum class MenuType {
    BOOL,
    INT,
    FLOAT
};

struct MenuItem {
    sf::Text text;
    MenuType type;
    union {
        bool b;
        int32_t i;
        float f;
    };
};

struct Menu {
    std::vector<MenuItem> items;
    int32_t item_index;
    int8_t dir_active;
    bool select_active;
};

void menu_int(Menu &menu, const char *msg, int32_t &i) {
    if (menu.item_index == menu.items.size() && menu.select_active) {
        i += menu.dir_active;
    }

    std::stringstream ss;
    ss << msg << " " << i;

    MenuItem item{};
    item.text.setString(ss.str());
    menu.items.push_back(item);
}

void menu_bool(Menu &menu, const char *msg, bool &b) {
    if (menu.item_index == menu.items.size() && menu.select_active) {
        b = !b;
        menu.select_active = false;
    }

    std::stringstream ss;
    ss << msg << " " << b;

    MenuItem item{};
    item.text.setString(ss.str());
    menu.items.push_back(item);
}

void menu_float(Menu &menu, const char *msg, float &f) {
    if (menu.item_index == menu.items.size() && menu.select_active) {
        f += (float)menu.dir_active;
    }

    std::stringstream ss;
    ss << msg << " " << f;

    MenuItem item{};
    item.text.setString(ss.str());
    menu.items.push_back(item);
}

// ?

std::vector<sf::RectangleShape> walls;
bool drawing_shape = false;
sf::Vector2i mouse_pos;
sf::Vector2f shape_anchor;
sf::Vector2f shape_corner;
sf::RectangleShape active_shape;

constexpr uint32_t HISTORY_SIZE = 10*PARTICLES;
uint32_t history_active_size = 0;
std::array<sf::Vertex, HISTORY_SIZE> history;

std::array<sf::Vertex, 3*PARTICLES> particle_dots;

// Evo
float goal_radius = 20.0f;
sf::CircleShape goal_circle(goal_radius);
sf::Vector2f goal = {0.9f*WINDOW_WIDTH, 0.5f*WINDOW_HEIGHT};
sf::Vector2f start = {0.1f*WINDOW_WIDTH, 0.5f*WINDOW_HEIGHT};

void check_collisions(evo::Particle &p, uint32_t steps, const std::vector<sf::RectangleShape> &walls) {
    if (p.position.x <= 0 || p.position.x >= WINDOW_WIDTH ||
        p.position.y <= 0 || p.position.y >= WINDOW_HEIGHT) {
        p.hit_wall = true;
        return;
    }

    for(auto& rect : walls){
        const auto& bounds = rect.getGlobalBounds();
        if(bounds.contains(p.position)) {
            p.hit_wall = true;
            return;
        }
    }

    if(norm(p.position - goal) <= goal_radius){
        assert(steps > 0);
        p.hit_goal = true;
        p.steps_to_goal = steps;
        return;
    }
}

int main() {
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

    evo::initialize(start, goal, goal_radius);

    //
    // LOOPY
    //

    Menu menu = {};

    while(window.isOpen()){
        menu.items.clear();
        menu.dir_active = 0;

        sf::Event event;
        while(window.pollEvent(event)){
            if(event.type == sf::Event::Closed)
                window.close();
            else if(event.type == sf::Event::KeyPressed){
                switch(event.key.code){
                case sf::Keyboard::Escape:
                    window.close();
                    break;
                case sf::Keyboard::Space:
                    evo::toggle_collect_debug_data();
                    break;
                case sf::Keyboard::Up:
                    menu.dir_active += 1;
                    break;
                case sf::Keyboard::Down:
                    menu.dir_active -= 1;
                    break;
                case sf::Keyboard::Enter:
                    menu.select_active = !menu.select_active;
                    break;
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

            {PROFILE_SCOPE("evo_fitness")
                evo::evaluate_fitness();
            }

            {PROFILE_SCOPE("evo_crossover")
                evo::crossover();
            }

            {PROFILE_SCOPE("evo_mutation")
                evo::mutate();
            }

            auto& debug = evo::debug_data();
            if(debug.active){
                PROFILE_SCOPE("evo_debug")
                    for(uint32_t x = 0; x < GRID_WIDTH; x++){
                        for(uint32_t y = 0; y < GRID_HEIGHT; y++){
                            auto& vec = evo::vector_in_field(debug.max_fitness_index, x, y);
                            auto speed = norm(vec);
                            auto unit = vec/speed;
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
                    float f = evo::normalized_fitness_for_particle(i);
                    if (f > 0.0f){
                        debug.selection_probs[4*debug.selection_probs_active_size+0] = sf::Vertex(pos, 													sf::Color::Red);
                        debug.selection_probs[4*debug.selection_probs_active_size+1] = sf::Vertex(pos+sf::Vector2f{(float)WIDTH*f,0},  sf::Color::Blue);
                        debug.selection_probs[4*debug.selection_probs_active_size+2] = sf::Vertex(pos+sf::Vector2f{(float)WIDTH*f,HEIGHT}, sf::Color::Blue);
                        debug.selection_probs[4*debug.selection_probs_active_size+3] = sf::Vertex(pos+sf::Vector2f{0,HEIGHT}, 			sf::Color::Red);
                        debug.selection_probs_active_size++;
                        pos.x += WIDTH*f;
                    }
                }


            }

            evo::next_generation();
        }

        if (!menu.select_active && menu.dir_active != 0) {
            menu.item_index += -menu.dir_active;
            if (menu.item_index < 0)
                menu.item_index = 0;
        }

        menu_int(menu, "Steps/frame:", draw_every);
        menu_int(menu, "Draw top:", draw_top);

        menu_bool(menu, "Draw trail:", draw_trail);
        menu_bool(menu, "Show profile:", show_profile);
        menu_bool(menu, "Show fitness %:", show_fitness_percentage);
        menu_bool(menu, "Show evo metric:", show_evo_metric);
        menu_bool(menu, "Show best vector field:", show_best_vector_field);

        menu_int(menu, "Num. individuals:", particle_count);
        menu_float(menu, "Max spawn speed:", max_gen_speed);
        menu_float(menu, "Max speed:", max_speed);
        menu_float(menu, "Mutation rate:", mutation_rate);
        menu_int(menu, "Steps/generation:", particle_count);

        auto& debug = evo::debug_data();
        ui_stream.clear();
        ui_stream.str(std::string{});

        ui_stream
            << "Draw every:      "  << draw_every << "frames \n"
            << "Generation:      "	<< debug.current_generation << "\n"
            << "Cycle:           "	<< steps << "/" << CYCLE << "\n"
            << "Max fitness:     "	<< 1.0f/debug.total_fitness << "%\n"
            << "Total fitness:   "	<< debug.total_fitness << "\n"
            << "Mutation rate:   "	<< MUTATION_RATE << "%\n"
            << "Mutations (new): "	<< MUTATIONS_TO_PERFORM << "\n";

        ui_stream << "Profiling: \n";
        for(auto& [name, time] : profile::data){
            ui_stream << name << "  " << std::left << time << "\n";
        }

        //
        // UPDATE
        //
        {
            PROFILE_SCOPE("update")
            auto &particles = evo::particle_data();
            for (uint32_t i = 0; i < PARTICLES; i++) {

                auto& p = particles[i];

                check_collisions(p, steps, walls);

                // update movement
                if(!p.hit_wall && !p.hit_goal){
                    uint32_t x = static_cast<uint32_t>(GRID_WIDTH*p.position.x/WINDOW_WIDTH);
                    uint32_t y = static_cast<uint32_t>(GRID_HEIGHT*p.position.y/WINDOW_HEIGHT);
                    auto& vec = evo::vector_in_field(i, x, y);

                    p.velocity += 0.1f*evo::mass_for_particle(i) * vec;

                    float l = norm(p.velocity);
                    if(l > MAX_SPEED){
                        p.velocity *= (MAX_SPEED/l);
                    }

                    history[history_active_size++%(HISTORY_SIZE)] = sf::Vertex(p.position, sf::Color(255,50,50,100));
                    p.position += p.velocity;
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

        static uint32_t frame = 0;
        if(frame++ % draw_every == 0){
            {PROFILE_SCOPE("draw")
                window.clear(sf::Color::White);
                auto& debug = evo::debug_data();
                if(debug.active){
                    auto float_rect = text.getLocalBounds();
                    sf::Vector2f offset = {10,10};
                    text_rect.setPosition(sf::Vector2f(float_rect.left-offset.x, float_rect.top-offset.y) + text.getPosition());
                    text_rect.setSize({float_rect.width+2*offset.x, float_rect.height+2*offset.y});
                    window.draw(debug.arrows.data(), debug.arrows.size(), sf::Lines);
                }
                window.draw(goal_circle);

                for(auto& rect : walls){
                    window.draw(rect);
                }
                if(drawing_shape){
                    window.draw(active_shape);
                }

                if (false) {
                    constexpr uint32_t size = 10;

                    static std::array<sf::Vertex, 4*(WINDOW_WIDTH/size)*(WINDOW_HEIGHT/size)> quads;
                    static std::array<float, (WINDOW_WIDTH/size)*(WINDOW_HEIGHT/size)> fitness_values;

                    float fitness_max = 0.0f;
                    for (uint32_t i = 0; i < WINDOW_WIDTH/size; ++i) {
                        for (uint32_t j = 0; j < WINDOW_HEIGHT/size; ++j) {
                            const uint32_t index = i + j*WINDOW_WIDTH/size;
                            const uint32_t x = i*size;
                            const uint32_t y = j*size;
                            evo::Particle p;
                            p.hit_goal      = false;
                            p.hit_wall      = false;
                            p.position      = sf::Vector2f(x, y);
                            p.velocity      = {0.0f, 0.0f};
                            p.acceleration  = {0.0f, 0.0f};
                            p.steps_to_goal = CYCLE;
                            check_collisions(p, CYCLE, walls);
                            auto score = fitness_function(p);
                            fitness_values[index] = score;
                            if (score > fitness_max)
                                fitness_max = score;
                        }
                    }

                    if (fitness_max > 0.0f)
                        for (auto &v : fitness_values)
                            v /= fitness_max;

                    for (uint32_t i = 0; i < WINDOW_WIDTH/size; ++i) {
                        for (uint32_t j = 0; j < WINDOW_HEIGHT/size; ++j) {
                            const uint32_t index = i + j*WINDOW_WIDTH/size;
                            const uint32_t x = i*size;
                            const uint32_t y = j*size;
                            uint32_t col_comp = 255.0f*fitness_values[index];
                            sf::Color col(col_comp, col_comp, col_comp, 255);
                            printf("%f\n", fitness_values[index]);
                            quads[4*index+0] = sf::Vertex(sf::Vector2f(     x,      y), col);
                            quads[4*index+1] = sf::Vertex(sf::Vector2f(x+size,      y), col);
                            quads[4*index+2] = sf::Vertex(sf::Vector2f(x+size, y+size), col);
                            quads[4*index+3] = sf::Vertex(sf::Vector2f(     x, y+size), col);
                        }
                    }
                    window.draw(quads.data(), quads.size(), sf::Quads);
                }

                window.draw(&history[0], (history_active_size > history.size()) ? history.size() : history_active_size, sf::Points);
                window.draw(&particle_dots[0], particle_dots.size(), sf::Triangles);

                //if(debug.active){
                //    text.setString(ui_stream.str());
                //    window.draw(text_rect);
                //    window.draw(text);
                //    window.draw(&debug.selection_probs[0], 4*debug.selection_probs_active_size, sf::Quads);
                //}

                float y = 25;
                menu.item_index = std::min((size_t) menu.item_index, menu.items.size()-1);
                for (int32_t i = 0; i < menu.items.size(); ++i) {
                    auto &item = menu.items[i];
                    bool selected = (menu.item_index == i);

                    auto col = sf::Color::Black;
                    if (selected) {
                        col = (menu.select_active) ? sf::Color::Blue : sf::Color::Red;
                    }

                    item.text.setFont(font);
                    item.text.setCharacterSize(16);
                    item.text.setFillColor(col);
                    item.text.setPosition({50,y});
                    y += 25.0f;
                    window.draw(item.text);
                }

                window.display();
            }
        }

    }

    return 0;
}
