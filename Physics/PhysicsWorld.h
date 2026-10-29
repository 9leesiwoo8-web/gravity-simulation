#pragma once

#include "Particle.h"
#include "GpuGravity.h"
#include "../Config/Config.h"

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

struct CollisionEvent {
    Vector2 position;
    double radius;
    Type type;
    bool fragmented;
};

enum class Integrator {
    VERLET,
    EULER
};

class PhysicsWorld {
private:
    std::vector<Particle> particles;
    double G;
    double softening;

    Integrator integrator = Integrator::VERLET;
    bool closed_system = false;
    bool gpu_allowed = true;

    std::uint64_t next_id = 0;

    std::mt19937 rng;
    std::vector<CollisionEvent> events;

    bool accel_dirty = true;
    bool collision_pending = false;

    std::shared_ptr<GpuGravity> gpu;

public:
    PhysicsWorld();
    PhysicsWorld(double G, double softening = Config::SOFTENING);

    const std::vector<Particle>& get_particles() const;

    void add_Particle(const Particle& particle);

    void set_integrator(Integrator integrator);
    void set_closed_system(bool closed);
    void set_gpu_allowed(bool allowed);

    double kinetic_energy() const;
    double potential_energy() const;
    double total_energy() const;

    void update(double dt);

    void update(double dt, int steps);

    std::vector<CollisionEvent> take_events();

private:
    void update_euler(double dt);
    void calculate_forces();
    void resolve_collisions();
    void remove_escaped();

    bool should_fragment(const Particle& a, const Particle& b) const;
    void spawn_fragments(const Particle& a, const Particle& b, double touch,
                         std::vector<Particle>& out);
};
