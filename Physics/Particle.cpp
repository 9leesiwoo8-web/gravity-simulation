#include "Particle.h"

#include "../Config/Config.h"

#include <cmath>

Particle::Particle()
    :   id(0), mass(1.0), radius(0.0), type(Type::PLANET), position(), velocity(), acceleration(), force()
{
    update_radius();
}

Particle::Particle(double mass, Type type, const Vector2& position, const Vector2& velocity)
    :   id(0), mass(mass), radius(0.0), type(type), position(position), velocity(velocity), acceleration(), force()
{
    update_radius();
}

void Particle::update_radius() {
    radius = Config::RADIUS_BASE + Config::RADIUS_PER_CBRT_MASS * std::cbrt(mass);
}

int Particle::rank() const {
    switch(type) {
    case Type::ASTEROID: return 1;
    case Type::PLANET:   return 2;
    case Type::GIANT:    return 3;
    default:             return 4;
    }
}

void Particle::set_id(std::uint64_t id) {
    this->id = id;
}

void Particle::set_mass(double mass) {
    this->mass = mass;
    update_radius();
}

void Particle::set_type(Type type) {
    this->type = type;
}

void Particle::set_position(const Vector2& position) {
    this->position = position;
}

void Particle::set_velocity(const Vector2& velocity) {
    this->velocity = velocity;
}

void Particle::refresh_acceleration() {
    acceleration = force / mass;
    clear_force();
}

void Particle::advance_position(double dt) {
    position += velocity * dt + acceleration * (0.5 * dt * dt);
}

void Particle::advance_velocity(double dt) {
    Vector2 new_acceleration = force / mass;

    velocity += (acceleration + new_acceleration) * (0.5 * dt);
    acceleration = new_acceleration;

    clear_force();
}

void Particle::advance_euler(double dt) {
    position += velocity * dt;
    velocity += acceleration * dt;
}
