#pragma once

#include <SFML/Graphics.hpp>

#include "../Config/Config.h"
#include "../Physics/PhysicsWorld.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// 사용자 입력으로 바뀌는 시뮬레이션 상태
struct SimControl {
    bool paused = false;
    int time_scale = Config::DEFAULT_TIME_SCALE;    // 프레임당 물리 스텝 수

    // 시나리오 선택 (F1~F9). 번호는 0부터 시작한다.
    int scenario = 0;
    std::string scenario_name;
    bool reset_requested = false;                   // 현재 시나리오를 처음부터 다시 시작

    int spawn_slot = Config::DEFAULT_SPAWN_SLOT;    // SPAWN_PRESETS 인덱스 + 1
    bool spawn_requested = false;
    Vector2 spawn_position;
    Vector2 spawn_velocity;
};

// 충돌 순간에 퍼져 나가는 링 형태의 이펙트
struct Effect {
    sf::Vector2f position;
    float radius;
    sf::Color color;
    bool fragmented;
    int age = 0;
    int life;
};

class Renderer {
private:
    sf::RenderWindow window;
    float scale;
    sf::Vector2f base_size;
    sf::View view;
    sf::Vector2f drag_start;
    bool dragging = false;          // 오른쪽 버튼으로 화면 이동 중

    bool launching = false;         // 왼쪽 버튼으로 천체 발사 조준 중
    sf::Vector2f launch_start;
    sf::Vector2i launch_pixel;

    std::vector<Effect> effects;

    bool show_trails = true;
    std::unordered_map<std::uint64_t, std::deque<sf::Vector2f>> trails;

    sf::Font font;
    std::optional<sf::Text> hud_text;
    sf::Clock fps_clock;
    float fps = 0.f;

public:
    Renderer();
    bool is_open() const;
    void process_events(SimControl& control);
    void draw(const PhysicsWorld& world, const SimControl& control);
    void clear_trails();
    void clear_effects();
    void add_effect(const CollisionEvent& event);

private:
    void update_view_size();
    void zoom_at(float factor, sf::Vector2i pixel);
    void handle_key(sf::Keyboard::Key code, SimControl& control);
    void update_trails(const PhysicsWorld& world);
    void draw_trails(const PhysicsWorld& world);
    void draw_effects();
    void draw_launch_preview(const SimControl& control);
    void draw_hud(const PhysicsWorld& world, const SimControl& control);
    static sf::Color color_of(Type type);
};
