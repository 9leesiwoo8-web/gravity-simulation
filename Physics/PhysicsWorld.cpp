#include "PhysicsWorld.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr double PI = 3.141592653589793;

    std::shared_ptr<GpuGravity> shared_gpu() {
        static std::shared_ptr<GpuGravity> instance = std::make_shared<GpuGravity>();
        return instance;
    }
}

PhysicsWorld::PhysicsWorld()
    :   PhysicsWorld(Config::G)
{}

PhysicsWorld::PhysicsWorld(double G, double softening)
    :   G(G), softening(softening), rng(std::random_device{}()),
        gpu(shared_gpu())
{}

const std::vector<Particle>& PhysicsWorld::get_particles() const {
    return particles;
}

void PhysicsWorld::add_Particle(const Particle& particle) {
    particles.push_back(particle);
    particles.back().set_id(next_id++);

    accel_dirty = true;
}

std::vector<CollisionEvent> PhysicsWorld::take_events() {
    std::vector<CollisionEvent> taken;
    taken.swap(events);
    return taken;
}

// 속도 Verlet:
//   1) 위치 갱신  2) 새 위치에서 힘 재계산  3) 속도 갱신
void PhysicsWorld::update(double dt) {
    if(accel_dirty) {
        calculate_forces();

        for(auto& p : particles) {
            p.refresh_acceleration();
        }

        accel_dirty = false;
    }

    for(auto& p : particles) {
        p.advance_position(dt);
    }

    calculate_forces();

    for(auto& p : particles) {
        p.advance_velocity(dt);
    }

    resolve_collisions();
    remove_escaped();
}

// GPU 한 번 호출에 묶는 최대 스텝 수 (한 번의 제출이 너무 길어지지 않게 한다)
namespace {
    constexpr int MAX_GPU_BATCH = 256;
}

void PhysicsWorld::update(double dt, int steps) {
    while(steps > 0) {
        bool use_gpu = gpu && gpu->available() && particles.size() >= Config::GPU_MIN_PARTICLES;

        if(use_gpu) {
            int done = 0;
            bool hit = false;

            // 가속도가 낡았으면 GPU가 먼저 새로 구한다 (update()의 첫 부분과 같다)
            if(gpu->advance(particles, dt, std::min(steps, MAX_GPU_BATCH), G, softening * softening,
                            accel_dirty, Config::GPU_COLLISION_DELAY_STEPS, done, hit)) {
                accel_dirty = false;
                steps -= done;

                // 겹침이 생기고 몇 스텝 뒤에 GPU가 멈췄다. 이후는 update()와 같다.
                if(hit) collision_pending = true;
                resolve_collisions();
                remove_escaped();
                continue;
            }
        }

        update(dt);
        steps--;
    }
}

// Plummer softening: F = G*m1*m2*d / (|d|^2 + eps^2)^(3/2)
// 가까이 지나갈 때 힘이 무한대로 발산하는 것을 막는다.
void PhysicsWorld::calculate_forces() {
    const double eps_sq = softening * softening;

    for(size_t i = 0; i < particles.size(); i++) {
        for(size_t j = i + 1; j < particles.size(); j++) {
            Particle& a = particles[i];
            Particle& b = particles[j];

            Vector2 direction = b.get_position() - a.get_position();
            double dist_sq = direction.length_squared();

            double touch = a.get_radius() + b.get_radius();
            if(dist_sq < touch * touch) {
                collision_pending = true;
            }

            double soft = dist_sq + eps_sq;
            double factor =
                G * a.get_mass() * b.get_mass() / (soft * std::sqrt(soft));

            Vector2 force = direction * factor;

            a.add_force(force);
            b.add_force(-force);
        }
    }
}

// 충돌 속도가 합쳐진 천체의 탈출 속도에 비해 충분히 크고,
// 두 천체의 질량이 비슷하면 합쳐지지 않고 파편으로 부서진다.
bool PhysicsWorld::should_fragment(const Particle& a, const Particle& b) const {
    double heavy = std::max(a.get_mass(), b.get_mass());
    double light = std::min(a.get_mass(), b.get_mass());

    if(heavy > Config::FRAGMENT_MAX_MASS_RATIO * light) return false;

    double total = a.get_mass() + b.get_mass();
    if(total / Config::FRAGMENT_COUNT < Config::MIN_FRAGMENT_MASS) return false;

    double touch = a.get_radius() + b.get_radius();
    double relative_speed = (a.get_velocity() - b.get_velocity()).length();
    double escape_speed = std::sqrt(2.0 * G * total / touch);

    return relative_speed > Config::FRAGMENT_SPEED_FACTOR * escape_speed;
}

// 두 천체의 질량, 운동량, 질량중심을 보존하면서 질량을 균등하게 나눈 파편을 만든다.
// 파편은 질량중심 둘레에 대칭으로 놓고 바깥쪽으로 같은 속도를 줘서 운동량이 유지된다.
void PhysicsWorld::spawn_fragments(const Particle& a, const Particle& b, double touch,
                                   std::vector<Particle>& out) {
    const double total = a.get_mass() + b.get_mass();
    const double fragment_mass = total / Config::FRAGMENT_COUNT;

    Vector2 center =
        (a.get_position() * a.get_mass() + b.get_position() * b.get_mass()) / total;
    Vector2 velocity =
        (a.get_velocity() * a.get_mass() + b.get_velocity() * b.get_mass()) / total;

    double relative_speed = (a.get_velocity() - b.get_velocity()).length();
    double outward_speed = Config::FRAGMENT_SPREAD_FACTOR * relative_speed;

    // 더 작은 쪽 등급을 물려받는다.
    Type type = a.rank() <= b.rank() ? a.get_type() : b.get_type();

    // 파편끼리 생성 즉시 다시 충돌하지 않도록 고리 반지름을 정한다.
    double fragment_radius =
        Particle(fragment_mass, type, Vector2(), Vector2()).get_radius();
    double ring = std::max(
        touch,
        2.0 * fragment_radius * 1.3 / (2.0 * std::sin(PI / Config::FRAGMENT_COUNT))
    );

    std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * PI);
    double start_angle = angle_dist(rng);

    for(int k = 0; k < Config::FRAGMENT_COUNT; k++) {
        double angle = start_angle + 2.0 * PI * k / Config::FRAGMENT_COUNT;
        Vector2 direction(std::cos(angle), std::sin(angle));

        out.emplace_back(
            fragment_mass,
            type,
            center + direction * ring,
            velocity + direction * outward_speed
        );
        out.back().set_id(next_id++);
    }

    events.push_back({center, touch * 0.5, type, true});
}

// 반지름이 겹치는 입자를 병합하거나 파편화한다. 어느 쪽이든 운동량과 질량중심을 보존한다.
void PhysicsWorld::resolve_collisions() {
    if(!collision_pending) return;
    collision_pending = false;

    std::vector<bool> removed(particles.size(), false);
    std::vector<Particle> fragments;
    bool changed = false;

    for(size_t i = 0; i < particles.size(); i++) {
        if(removed[i]) continue;

        Particle& a = particles[i];

        for(size_t j = i + 1; j < particles.size(); j++) {
            if(removed[j]) continue;

            const Particle& b = particles[j];

            double touch = a.get_radius() + b.get_radius();
            double dist_sq =
                (b.get_position() - a.get_position()).length_squared();

            if(dist_sq >= touch * touch) continue;

            changed = true;

            if(should_fragment(a, b)) {
                spawn_fragments(a, b, touch, fragments);

                removed[i] = true;
                removed[j] = true;
                break;
            }

            double total_mass = a.get_mass() + b.get_mass();

            Vector2 new_position =
                (a.get_position() * a.get_mass() +
                 b.get_position() * b.get_mass()) / total_mass;

            Vector2 new_velocity =
                (a.get_velocity() * a.get_mass() +
                 b.get_velocity() * b.get_mass()) / total_mass;

            if(b.rank() > a.rank()) a.set_type(b.get_type());
            if(b.get_mass() > a.get_mass()) a.set_id(b.get_id());

            a.set_mass(total_mass);
            a.set_position(new_position);
            a.set_velocity(new_velocity);

            events.push_back({new_position, touch * 0.5, a.get_type(), false});

            removed[j] = true;
        }
    }

    if(!changed) return;

    std::vector<Particle> survivors;
    survivors.reserve(particles.size() + fragments.size());

    for(size_t i = 0; i < particles.size(); i++) {
        if(!removed[i]) survivors.push_back(particles[i]);
    }

    survivors.insert(survivors.end(), fragments.begin(), fragments.end());

    particles = std::move(survivors);
    accel_dirty = true;
}

// 질량중심에서 너무 멀리 날아간 입자를 제거한다.
void PhysicsWorld::remove_escaped() {
    if(particles.empty()) return;

    double total_mass = 0.0;
    Vector2 center;

    for(const Particle& p : particles) {
        total_mass += p.get_mass();
        center += p.get_position() * p.get_mass();
    }
    center /= total_mass;

    const double limit_sq = Config::ESCAPE_RADIUS * Config::ESCAPE_RADIUS;

    auto escaped = [&](const Particle& p) {
        return (p.get_position() - center).length_squared() > limit_sq;
    };

    auto new_end = std::remove_if(particles.begin(), particles.end(), escaped);

    if(new_end != particles.end()) {
        particles.erase(new_end, particles.end());
        accel_dirty = true;
    }
}
