#pragma once

#include "Particle.h"
#include "GpuGravity.h"
#include "../Config/Config.h"

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

// 충돌이 일어났을 때 렌더러가 이펙트를 그리기 위한 정보
struct CollisionEvent {
    Vector2 position;
    double radius;
    Type type;
    bool fragmented;    // true면 병합이 아니라 파편화
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
    // true면 충돌(병합·파편화)과 궤도 이탈 제거를 끈다. 에너지가 보존되는 닫힌 계.
    bool closed_system = false;
    // false면 입자가 많아도 CPU(double)로만 계산한다. GPU는 힘을 float로 계산해
    // 적분 방식 간 에너지 오차 비교에 잡음이 섞이기 때문이다.
    bool gpu_allowed = true;

    std::uint64_t next_id = 0;

    std::mt19937 rng;
    std::vector<CollisionEvent> events;

    // 입자 구성이 바뀌어 저장된 가속도를 다시 계산해야 하는지
    bool accel_dirty = true;
    // 겹친 입자 쌍이 발견되어 충돌 처리가 필요한지
    bool collision_pending = false;

    // GPU 중력 계산기 (사용 불가하면 CPU로 계산한다).
    // 초기화가 느리므로 월드를 새로 만들어도 하나를 공유한다.
    std::shared_ptr<GpuGravity> gpu;

public:
    PhysicsWorld();
    PhysicsWorld(double G, double softening = Config::SOFTENING);

    const std::vector<Particle>& get_particles() const;

    void add_Particle(const Particle& particle);

    // 오일러는 항상 CPU로만 계산한다.
    void set_integrator(Integrator integrator);
    void set_closed_system(bool closed);
    void set_gpu_allowed(bool allowed);

    double kinetic_energy() const;
    // Plummer softening 중력과 같은 퍼텐셜: U = -G*m1*m2 / sqrt(|d|^2 + eps^2)
    double potential_energy() const;
    double total_energy() const;

    void update(double dt);

    // update(dt)를 steps번 한 것과 같다. GPU를 쓸 수 있으면 스텝마다 CPU와 주고받지 않고
    // 여러 스텝을 GPU에서 연속으로 진행한다.
    void update(double dt, int steps);

    // 마지막 호출 이후 발생한 충돌 이벤트를 꺼낸다.
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
