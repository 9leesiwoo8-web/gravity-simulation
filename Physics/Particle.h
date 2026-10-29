#pragma once

#include "../Math/Vector2.h"

#include <cstdint>

enum class Type {
    SUN,
    GIANT,
    PLANET,
    ASTEROID
};

class Particle {
private:
    std::uint64_t id;

    double mass;
    double radius;

    Type type;

    Vector2 position;
    Vector2 velocity;
    Vector2 acceleration;
    Vector2 force;

public:
    Particle();
    Particle(double mass, Type type, const Vector2& position, const Vector2& velocity);

    std::uint64_t get_id() const { return id; }
    double get_mass() const { return mass; }
    Type get_type() const { return type; }
    const Vector2& get_position() const { return position; }
    const Vector2& get_velocity() const { return velocity; }
    const Vector2& get_force() const { return force; }
    const Vector2& get_acceleration() const { return acceleration; }

    double get_radius() const { return radius; }

    int rank() const;

    void set_id(std::uint64_t id);
    void set_mass(double mass);
    void set_type(Type type);
    void set_position(const Vector2& position);
    void set_velocity(const Vector2& velocity);
    void set_acceleration(const Vector2& acceleration) { this->acceleration = acceleration; }

    void add_force(const Vector2& force) { this->force += force; }
    void clear_force() { force.clear(); }

private:
    void update_radius();

public:
    void refresh_acceleration();
    void advance_position(double dt);
    void advance_velocity(double dt);

    void advance_euler(double dt);
};
