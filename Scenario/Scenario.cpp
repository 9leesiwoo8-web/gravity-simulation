#include "Scenario.h"

#include "../Config/Config.h"

#include <cmath>
#include <random>

namespace {

    constexpr double PI = 3.141592653589793;

    // 중심 천체 주위를 도는 천체 무리의 설정
    struct Ring {
        Type type;
        int count;
        double min_radius, max_radius;
        double min_mass, max_mass;
    };

    // center 주위에 ring.count개의 천체를 원궤도 속도로 배치한다.
    //  central_mass     : 궤도 속도를 정할 때 쓰는 중심 질량
    //  center_velocity  : 중심 천체 자신의 속도 (모든 천체에 더해진다)
    //  direction        : +1 반시계, -1 시계 방향
    //  speed_variation  : 원궤도 속도에 곱하는 무작위 계수의 폭 (0.1이면 0.9~1.1배)
    void add_ring(
        std::vector<Particle>& out,
        std::mt19937& gen,
        const Ring& ring,
        const Vector2& center,
        const Vector2& center_velocity,
        double central_mass,
        int direction = 1,
        double speed_variation = 0.10)
    {
        std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * PI);
        std::uniform_real_distribution<double> radius_dist(ring.min_radius, ring.max_radius);
        std::uniform_real_distribution<double> mass_dist(ring.min_mass, ring.max_mass);
        std::uniform_real_distribution<double> speed_dist(1.0 - speed_variation, 1.0 + speed_variation);

        for(int i = 0; i < ring.count; i++)
        {
            double r = radius_dist(gen);
            double angle = angle_dist(gen);
            double mass = mass_dist(gen);

            double speed = std::sqrt(Config::G * central_mass / r) * speed_dist(gen);

            Vector2 offset(r * std::cos(angle), r * std::sin(angle));
            Vector2 tangent(-std::sin(angle), std::cos(angle));

            out.emplace_back(
                mass,
                ring.type,
                center + offset,
                center_velocity + tangent * (speed * direction)
            );
        }
    }

    // 전체 질량중심이 원점에 정지하도록 위치와 속도를 옮긴 뒤 world에 추가한다.
    // (계가 한쪽으로 흘러가거나 화면 밖으로 사라지는 것을 막는다)
    void add_to_world(PhysicsWorld& world, std::vector<Particle>& bodies)
    {
        double total_mass = 0.0;
        Vector2 center;
        Vector2 velocity;

        for(const Particle& p : bodies) {
            total_mass += p.get_mass();
            center += p.get_position() * p.get_mass();
            velocity += p.get_velocity() * p.get_mass();
        }

        center /= total_mass;
        velocity /= total_mass;

        for(Particle& p : bodies) {
            p.set_position(p.get_position() - center);
            p.set_velocity(p.get_velocity() - velocity);
            world.add_Particle(p);
        }
    }

    // center 주위 반지름 r, 각도 angle 위치에 원궤도 속도의 speed_factor배로 천체를 놓는다.
    // speed_factor < 1이면 타원 궤도가 된다. (비교 모드용, 난수 없음)
    void add_body(
        std::vector<Particle>& out,
        Type type,
        double mass,
        const Vector2& center,
        const Vector2& center_velocity,
        double central_mass,
        double r,
        double angle,
        double speed_factor)
    {
        double speed = std::sqrt(Config::G * central_mass / r) * speed_factor;

        Vector2 offset(r * std::cos(angle), r * std::sin(angle));
        Vector2 tangent(-std::sin(angle), std::cos(angle));

        out.emplace_back(mass, type, center + offset, center_velocity + tangent * speed);
    }

    // ---- F1: 태양계 ----
    void build_solar_system(PhysicsWorld& world)
    {
        constexpr double SUN_MASS = 5000.0;

        std::random_device rd;
        std::mt19937 gen(rd());

        std::vector<Particle> bodies;
        bodies.emplace_back(SUN_MASS, Type::SUN, Vector2(0, 0), Vector2(0, 0));

        const Vector2 origin;

        add_ring(bodies, gen, {Type::GIANT,    10, 250, 700, 120, 300}, origin, origin, SUN_MASS);
        add_ring(bodies, gen, {Type::PLANET,  150, 120, 600,   5,  20}, origin, origin, SUN_MASS);
        add_ring(bodies, gen, {Type::ASTEROID, 1000, 300, 550, 0.2, 2.0}, origin, origin, SUN_MASS);

        add_to_world(world, bodies);
    }

    // ---- F2: 쌍성계 ----
    // 두 별이 서로를 공전하고, 바깥에서 두 별을 함께 도는 행성들과
    // 한쪽 별 가까이를 도는 행성이 있다.
    void build_binary_stars(PhysicsWorld& world)
    {
        constexpr double STAR_MASS = 2500.0;
        constexpr double SEPARATION = 300.0;

        std::random_device rd;
        std::mt19937 gen(rd());

        // 같은 질량 두 별의 원궤도 속도: v^2 = G*m / (2*d)
        const double star_speed = std::sqrt(Config::G * STAR_MASS / (2.0 * SEPARATION));

        const Vector2 pos_a(-SEPARATION / 2.0, 0.0);
        const Vector2 pos_b( SEPARATION / 2.0, 0.0);
        const Vector2 vel_a(0.0,  star_speed);
        const Vector2 vel_b(0.0, -star_speed);

        std::vector<Particle> bodies;
        bodies.emplace_back(STAR_MASS, Type::SUN, pos_a, vel_a);
        bodies.emplace_back(STAR_MASS, Type::SUN, pos_b, vel_b);

        const Vector2 origin;
        const double total_star_mass = 2.0 * STAR_MASS;

        // 두 별을 함께 도는 천체 (별 사이 거리의 2.5배 바깥)
        add_ring(bodies, gen, {Type::GIANT,     6, 750, 1100, 120, 250}, origin, origin, total_star_mass, 1, 0.05);
        add_ring(bodies, gen, {Type::PLANET,   60, 700, 1100,   5,  20}, origin, origin, total_star_mass, 1, 0.05);
        add_ring(bodies, gen, {Type::ASTEROID, 600, 700, 1000, 0.2, 1.5}, origin, origin, total_star_mass, 1, 0.05);

        // 한쪽 별 가까이를 도는 행성 (힐 반지름 안쪽)
        add_ring(bodies, gen, {Type::PLANET, 3, 45, 70, 5, 12}, pos_a, vel_a, STAR_MASS, 1, 0.03);

        add_to_world(world, bodies);
    }

    // ---- F3: 은하 충돌 ----
    // 서로 반대 방향으로 회전하는 두 소형 은하가 마주 보고 날아온다.
    void build_galaxy_collision(PhysicsWorld& world)
    {
        constexpr double CORE_MASS = 3000.0;

        std::random_device rd;
        std::mt19937 gen(rd());

        const Vector2 pos_a(-600.0, -120.0);
        const Vector2 pos_b( 600.0,  120.0);
        const Vector2 vel_a( 25.0, 0.0);
        const Vector2 vel_b(-25.0, 0.0);

        std::vector<Particle> bodies;
        bodies.emplace_back(CORE_MASS, Type::SUN, pos_a, vel_a);
        bodies.emplace_back(CORE_MASS, Type::SUN, pos_b, vel_b);

        add_ring(bodies, gen, {Type::PLANET,   80, 80, 300,  5,  20}, pos_a, vel_a, CORE_MASS,  1, 0.05);
        add_ring(bodies, gen, {Type::ASTEROID, 600, 60, 320, 0.3, 2.0}, pos_a, vel_a, CORE_MASS,  1, 0.05);

        add_ring(bodies, gen, {Type::PLANET,   80, 80, 300,  5,  20}, pos_b, vel_b, CORE_MASS, -1, 0.05);
        add_ring(bodies, gen, {Type::ASTEROID, 600, 60, 320, 0.3, 2.0}, pos_b, vel_b, CORE_MASS, -1, 0.05);

        add_to_world(world, bodies);
    }

    // ---- F4: 소행성 벨트 ----
    // 좁은 띠에 소행성이 촘촘히 돌고, 안팎에 행성과 거대 행성이 있다.
    void build_asteroid_belt(PhysicsWorld& world)
    {
        constexpr double SUN_MASS = 5000.0;

        std::random_device rd;
        std::mt19937 gen(rd());

        std::vector<Particle> bodies;
        bodies.emplace_back(SUN_MASS, Type::SUN, Vector2(0, 0), Vector2(0, 0));

        const Vector2 origin;

        add_ring(bodies, gen, {Type::ASTEROID, 2000, 350, 450, 0.2, 1.0}, origin, origin, SUN_MASS, 1, 0.03);
        add_ring(bodies, gen, {Type::PLANET,    20, 130, 300,   5,  20}, origin, origin, SUN_MASS, 1, 0.03);
        add_ring(bodies, gen, {Type::GIANT,      2, 600, 700, 200, 300}, origin, origin, SUN_MASS, 1, 0.03);

        add_to_world(world, bodies);
    }

    // ---- F5: 샌드박스 ----
    // 태양만 있다. 마우스로 천체를 던져서 직접 꾸민다.
    void build_sandbox(PhysicsWorld& world)
    {
        world.add_Particle(Particle(5000.0, Type::SUN, Vector2(0, 0), Vector2(0, 0)));
    }


    // ---- 비교 F1: 타원 궤도 ----
    // 태양 하나와 길쭉한 타원 궤도의 행성 하나. 오일러의 에너지 증가가 가장 뚜렷하다.
    void build_compare_eccentric(PhysicsWorld& world)
    {
        constexpr double SUN_MASS = 5000.0;

        std::vector<Particle> bodies;
        bodies.emplace_back(SUN_MASS, Type::SUN, Vector2(0, 0), Vector2(0, 0));

        const Vector2 origin;
        add_body(bodies, Type::PLANET, 10.0, origin, origin, SUN_MASS, 300.0, 0.0, 0.7);

        add_to_world(world, bodies);
    }

    // ---- 비교 F2: 작은 태양계 ----
    void build_compare_solar_system(PhysicsWorld& world)
    {
        constexpr double SUN_MASS = 5000.0;

        std::vector<Particle> bodies;
        bodies.emplace_back(SUN_MASS, Type::SUN, Vector2(0, 0), Vector2(0, 0));

        const Vector2 origin;

        struct Planet { Type type; double mass; double r; double speed_factor; };
        const Planet planets[] = {
            {Type::PLANET,    5.0, 100.0, 1.00},
            {Type::PLANET,   10.0, 150.0, 0.90},
            {Type::PLANET,    8.0, 220.0, 1.00},
            {Type::PLANET,   20.0, 300.0, 0.85},
            {Type::GIANT,   150.0, 380.0, 1.00},
            {Type::GIANT,   100.0, 450.0, 0.95},
            {Type::PLANET,   30.0, 520.0, 1.00},
            {Type::PLANET,   40.0, 600.0, 0.90},
        };

        double angle = 0.0;
        for(const Planet& p : planets) {
            add_body(bodies, p.type, p.mass, origin, origin, SUN_MASS, p.r, angle, p.speed_factor);
            angle += 0.9;
        }

        add_to_world(world, bodies);
    }

    // ---- 비교 F3: 쌍성계 ----
    void build_compare_binary(PhysicsWorld& world)
    {
        constexpr double STAR_MASS = 2500.0;
        constexpr double SEPARATION = 300.0;

        const double star_speed = std::sqrt(Config::G * STAR_MASS / (2.0 * SEPARATION));

        const Vector2 pos_a(-SEPARATION / 2.0, 0.0);
        const Vector2 pos_b( SEPARATION / 2.0, 0.0);
        const Vector2 vel_a(0.0,  star_speed);
        const Vector2 vel_b(0.0, -star_speed);

        std::vector<Particle> bodies;
        bodies.emplace_back(STAR_MASS, Type::SUN, pos_a, vel_a);
        bodies.emplace_back(STAR_MASS, Type::SUN, pos_b, vel_b);

        const Vector2 origin;
        add_body(bodies, Type::PLANET, 10.0, origin, origin, 2.0 * STAR_MASS, 750.0, 0.0, 1.0);
        add_body(bodies, Type::PLANET, 10.0, origin, origin, 2.0 * STAR_MASS, 950.0, 2.5, 1.0);
        add_body(bodies, Type::PLANET,  5.0, pos_a, vel_a, STAR_MASS, 60.0, 1.0, 1.0);

        add_to_world(world, bodies);
    }


    // ---- 비교 F4: 천체 300개 ----
    // 태양, 행성 몇 개, 소행성 약 290개. 시드를 고정해 두 world가 같은 배치로 시작한다.
    void build_compare_crowd(PhysicsWorld& world)
    {
        constexpr double SUN_MASS = 5000.0;

        std::mt19937 gen(20240601);

        std::vector<Particle> bodies;
        bodies.emplace_back(SUN_MASS, Type::SUN, Vector2(0, 0), Vector2(0, 0));

        const Vector2 origin;

        add_ring(bodies, gen, {Type::GIANT,      2, 500, 650, 120, 200}, origin, origin, SUN_MASS, 1, 0.05);
        add_ring(bodies, gen, {Type::PLANET,     7, 150, 450,   5,  20}, origin, origin, SUN_MASS, 1, 0.05);
        add_ring(bodies, gen, {Type::ASTEROID, 290, 200, 480, 0.2, 1.5}, origin, origin, SUN_MASS, 1, 0.10);

        add_to_world(world, bodies);
    }

}

const std::vector<Scenario>& compare_scenarios()
{
    static const std::vector<Scenario> list = {
        {"Eccentric Orbit",   build_compare_eccentric},
        {"Mini Solar System", build_compare_solar_system},
        {"Binary Stars",      build_compare_binary},
        {"300 Bodies",        build_compare_crowd},
    };

    return list;
}

const std::vector<Scenario>& scenarios()
{
    static const std::vector<Scenario> list = {
        {"Solar System",      build_solar_system},
        {"Binary Stars",      build_binary_stars},
        {"Galaxy Collision",  build_galaxy_collision},
        {"Asteroid Belt",     build_asteroid_belt},
        {"Sandbox (Sun only)", build_sandbox},
    };

    return list;
}
