#pragma once

#include <SFML/Graphics.hpp>

#include "../Config/Config.h"
#include "../Physics/EnergyLog.h"
#include "../Physics/PhysicsWorld.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

enum class Mode {
    SIMULATION,
    COMPARE
};

struct SimControl {
    Mode mode = Mode::SIMULATION;
    bool mode_changed = false;

    bool paused = false;
    int time_scale = Config::DEFAULT_TIME_SCALE;
    int compare_time_scale = Config::COMPARE_DEFAULT_TIME_SCALE;

    int compare_scenario = 0;
    std::string compare_scenario_name;
    bool log_graph = false;

    bool save_requested = false;
    std::string status_message;
    int status_frames = 0;

    int scenario = 0;
    std::string scenario_name;
    bool reset_requested = false;

    int spawn_slot = Config::DEFAULT_SPAWN_SLOT;
    bool spawn_requested = false;
    Vector2 spawn_position;
    Vector2 spawn_velocity;
};

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
    bool dragging = false;

    bool launching = false;
    sf::Vector2f launch_start;
    sf::Vector2i launch_pixel;

    std::vector<Effect> effects;

    using TrailMap = std::unordered_map<std::uint64_t, std::deque<sf::Vector2f>>;

    bool show_trails = true;
    TrailMap trails;
    TrailMap compare_trails[2];

    sf::Font font;
    std::optional<sf::Text> hud_text;
    sf::Clock fps_clock;
    float fps = 0.f;

public:
    Renderer();
    bool is_open() const;
    void process_events(SimControl& control);
    void draw(const PhysicsWorld& world, const SimControl& control);
    void draw_compare(const PhysicsWorld& euler, const PhysicsWorld& verlet,
                      const EnergyLog& log, const SimControl& control);
    void clear_trails();
    void clear_compare_trails();
    void clear_effects();
    void add_effect(const CollisionEvent& event);

private:
    void update_view_size();
    void zoom_at(float factor, sf::Vector2i pixel);
    void handle_key(sf::Keyboard::Key code, SimControl& control);
    void update_trails(const PhysicsWorld& world, TrailMap& trails);
    void draw_trails(const PhysicsWorld& world, const TrailMap& trails);
    void draw_bodies(const PhysicsWorld& world);
    void draw_effects();
    void draw_launch_preview(const SimControl& control);
    void draw_hud(const PhysicsWorld& world, const SimControl& control);
    void update_fps();
    void draw_text(const std::string& text, sf::Vector2f position, sf::Color color,
                   unsigned size = 14, bool right_align = false);
    sf::FloatRect mode_button(int index) const;
    void draw_mode_buttons(const SimControl& control);
    void draw_compare_overlay(const EnergyLog& log, const SimControl& control);
    void draw_energy_graph(const EnergyLog& log, const SimControl& control, sf::FloatRect area);
    static sf::Color color_of(Type type);
};
