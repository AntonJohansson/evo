#pragma once

#include <SFML/System/Vector2.hpp>
#include <math.h>

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

