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

    // 안쪽 루프에서 자주 불리는 getter는 인라인으로 둔다.
    std::uint64_t get_id() const { return id; }
    double get_mass() const { return mass; }
    Type get_type() const { return type; }
    const Vector2& get_position() const { return position; }
    const Vector2& get_velocity() const { return velocity; }
    const Vector2& get_force() const { return force; }
    const Vector2& get_acceleration() const { return acceleration; }

    // 질량에 비례하는 반지름 (충돌 판정과 렌더링에 공통으로 사용)
    double get_radius() const { return radius; }

    // 클수록 큰 천체 (SUN > GIANT > PLANET > ASTEROID)
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
    // 속도 Verlet 적분
    void refresh_acceleration();
    void advance_position(double dt);
    void advance_velocity(double dt);

    // 명시적 오일러 적분 (비교용)
    void advance_euler(double dt);
};
