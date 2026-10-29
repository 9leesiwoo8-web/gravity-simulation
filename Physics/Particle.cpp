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

// 반지름은 질량이 바뀔 때만 다시 계산한다 (충돌 판정에서 매번 cbrt를 부르지 않도록)
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

// 현재 누적된 힘으로 가속도를 초기화한다 (첫 스텝, 병합 직후에 사용)
void Particle::refresh_acceleration() {
    acceleration = force / mass;
    clear_force();
}

// x(t+dt) = x + v*dt + 0.5*a(t)*dt^2
void Particle::advance_position(double dt) {
    position += velocity * dt + acceleration * (0.5 * dt * dt);
}

// v(t+dt) = v + 0.5*(a(t) + a(t+dt))*dt
void Particle::advance_velocity(double dt) {
    Vector2 new_acceleration = force / mass;

    velocity += (acceleration + new_acceleration) * (0.5 * dt);
    acceleration = new_acceleration;

    clear_force();
}
