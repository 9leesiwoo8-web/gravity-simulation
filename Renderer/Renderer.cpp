#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_set>

Renderer::Renderer()
    :   window(sf::VideoMode({Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT}), "Gravity Simulation"),
        scale(Config::DEFAULT_SCALE),
        base_size(static_cast<float>(Config::WINDOW_WIDTH),
                  static_cast<float>(Config::WINDOW_HEIGHT)),
        view(sf::Vector2f(0.f, 0.f),
             sf::Vector2f(Config::WINDOW_WIDTH / Config::DEFAULT_SCALE,
                          Config::WINDOW_HEIGHT / Config::DEFAULT_SCALE))
{
    window.setFramerateLimit(Config::FRAMERATE_LIMIT);
    window.setView(view);

    if(font.openFromFile("C:/Windows/Fonts/consola.ttf") ||
       font.openFromFile("C:/Windows/Fonts/arial.ttf")) {
        hud_text.emplace(font, "", 14);
        hud_text->setFillColor(sf::Color(200, 200, 200));
        hud_text->setPosition({10.f, 8.f});
    }
}

bool Renderer::is_open() const {
    return window.isOpen();
}

void Renderer::clear_trails() {
    trails.clear();
}

void Renderer::clear_compare_trails() {
    compare_trails[0].clear();
    compare_trails[1].clear();
}

void Renderer::clear_effects() {
    effects.clear();
}

void Renderer::add_effect(const CollisionEvent& event) {
    if(effects.size() >= Config::MAX_EFFECTS) return;

    Effect effect;
    effect.position = sf::Vector2f(
        static_cast<float>(event.position.x),
        static_cast<float>(event.position.y)
    );
    effect.radius = static_cast<float>(event.radius);
    effect.fragmented = event.fragmented;
    effect.color = event.fragmented ? sf::Color(255, 170, 40) : color_of(event.type);
    effect.life = event.fragmented ? Config::FRAGMENT_EFFECT_LIFE : Config::MERGE_EFFECT_LIFE;

    effects.push_back(effect);
}

void Renderer::update_view_size() {
    view.setSize({base_size.x / scale, base_size.y / scale});
    window.setView(view);
}

void Renderer::zoom_at(float factor, sf::Vector2i pixel) {
    sf::Vector2f before = window.mapPixelToCoords(pixel, view);

    scale = std::clamp(scale * factor, Config::MIN_SCALE, Config::MAX_SCALE);
    update_view_size();

    sf::Vector2f after = window.mapPixelToCoords(pixel, view);

    view.move(before - after);
    window.setView(view);
}

void Renderer::handle_key(sf::Keyboard::Key code, SimControl& control) {
    using Key = sf::Keyboard::Key;

    const int function_key = static_cast<int>(code) - static_cast<int>(Key::F1);
    if(function_key >= 0 && function_key < 9) {
        if(control.mode == Mode::COMPARE) control.compare_scenario = function_key;
        else control.scenario = function_key;

        control.reset_requested = true;
        return;
    }

    int& time_scale =
        control.mode == Mode::COMPARE ? control.compare_time_scale : control.time_scale;

    switch(code) {
    case Key::Tab:
        control.mode = control.mode == Mode::SIMULATION ? Mode::COMPARE : Mode::SIMULATION;
        control.mode_changed = true;
        launching = false;
        break;
    case Key::L:
        control.log_graph = !control.log_graph;
        break;
    case Key::S:
        if(control.mode == Mode::COMPARE) control.save_requested = true;
        break;
    case Key::Space:
        control.paused = !control.paused;
        break;
    case Key::Equal:
    case Key::Add:
        time_scale = std::min(time_scale * 2, Config::MAX_TIME_SCALE);
        break;
    case Key::Hyphen:
    case Key::Subtract:
        time_scale = std::max(time_scale / 2, Config::MIN_TIME_SCALE);
        break;
    case Key::R:
        control.reset_requested = true;
        break;
    case Key::Num1:
    case Key::Numpad1:
        control.spawn_slot = 1;
        break;
    case Key::Num2:
    case Key::Numpad2:
        control.spawn_slot = 2;
        break;
    case Key::Num3:
    case Key::Numpad3:
        control.spawn_slot = 3;
        break;
    case Key::Num4:
    case Key::Numpad4:
        control.spawn_slot = 4;
        break;
    case Key::Escape:
        launching = false;
        break;
    case Key::T:
        show_trails = !show_trails;
        if(!show_trails) trails.clear();
        break;
    default:
        break;
    }
}

void Renderer::process_events(SimControl& control) {
    while(const std::optional event = window.pollEvent()) {

        if(event->is<sf::Event::Closed>()) {
            window.close();
        }

        if(const auto* resized = event->getIf<sf::Event::Resized>()) {
            base_size = sf::Vector2f(resized->size);
            update_view_size();
        }

        if(const auto* key = event->getIf<sf::Event::KeyPressed>()) {
            handle_key(key->code, control);
        }

        if(const auto* scroll = event->getIf<sf::Event::MouseWheelScrolled>()) {
            sf::Vector2i anchor = control.mode == Mode::COMPARE
                ? sf::Vector2i(window.getSize() / 2u)
                : scroll->position;

            zoom_at(scroll->delta > 0 ? Config::ZOOM_IN_FACTOR : Config::ZOOM_OUT_FACTOR, anchor);
        }

        if(const auto* pressed = event->getIf<sf::Event::MouseButtonPressed>()) {
            const sf::Vector2f click(pressed->position);

            if(pressed->button == sf::Mouse::Button::Left &&
               (mode_button(0).contains(click) || mode_button(1).contains(click))) {
                Mode target = mode_button(0).contains(click) ? Mode::SIMULATION : Mode::COMPARE;

                if(target != control.mode) {
                    control.mode = target;
                    control.mode_changed = true;
                }
            }
            else if(pressed->button == sf::Mouse::Button::Left) {
                launching = control.mode == Mode::SIMULATION;
                launch_start = window.mapPixelToCoords(pressed->position, view);
                launch_pixel = pressed->position;
            }
            else if(pressed->button == sf::Mouse::Button::Right) {
                dragging = true;
                drag_start = window.mapPixelToCoords(pressed->position, view);
            }
        }

        if(const auto* released = event->getIf<sf::Event::MouseButtonReleased>()) {
            if(released->button == sf::Mouse::Button::Left && launching) {
                launching = false;

                sf::Vector2f end = window.mapPixelToCoords(released->position, view);
                sf::Vector2f drag = end - launch_start;

                control.spawn_requested = true;
                control.spawn_position = Vector2(launch_start.x, launch_start.y);
                control.spawn_velocity =
                    Vector2(drag.x, drag.y) * Config::LAUNCH_SPEED_FACTOR;
            }
            else if(released->button == sf::Mouse::Button::Right) {
                dragging = false;
            }
        }

        if(const auto* moved = event->getIf<sf::Event::MouseMoved>()) {
            if(launching) {
                launch_pixel = moved->position;
            }

            if(dragging) {
                sf::Vector2f current = window.mapPixelToCoords(moved->position, view);

                view.move(drag_start - current);
                window.setView(view);
            }
        }
    }
}

sf::Color Renderer::color_of(Type type) {
    switch(type) {
    case Type::SUN:      return sf::Color::Red;
    case Type::GIANT:    return sf::Color::Magenta;
    case Type::PLANET:   return sf::Color::Cyan;
    case Type::ASTEROID: return sf::Color::White;
    }
    return sf::Color::Green;
}

void Renderer::update_trails(const PhysicsWorld& world, TrailMap& trails) {
    std::unordered_set<std::uint64_t> alive;

    for(const Particle& p : world.get_particles()) {
        alive.insert(p.get_id());

        sf::Vector2f pos(
            static_cast<float>(p.get_position().x),
            static_cast<float>(p.get_position().y)
        );

        auto& trail = trails[p.get_id()];

        if(!trail.empty() && trail.back() == pos) continue;

        trail.push_back(pos);
        if(trail.size() > Config::TRAIL_LENGTH) trail.pop_front();
    }

    for(auto it = trails.begin(); it != trails.end();) {
        if(alive.count(it->first) == 0) it = trails.erase(it);
        else ++it;
    }
}

void Renderer::draw_trails(const PhysicsWorld& world, const TrailMap& trails) {
    for(const Particle& p : world.get_particles()) {
        auto it = trails.find(p.get_id());
        if(it == trails.end() || it->second.size() < 2) continue;

        const auto& trail = it->second;
        sf::Color base = color_of(p.get_type());

        sf::VertexArray line(sf::PrimitiveType::LineStrip, trail.size());
        for(std::size_t i = 0; i < trail.size(); i++) {
            float t = static_cast<float>(i) / static_cast<float>(trail.size() - 1);
            sf::Color c = base;
            c.a = static_cast<std::uint8_t>(10 + 110 * t);

            line[i].position = trail[i];
            line[i].color = c;
        }

        window.draw(line);
    }
}

void Renderer::draw_effects() {
    const float line_width = 2.f / scale;

    for(Effect& e : effects) {
        float t = static_cast<float>(e.age) / static_cast<float>(e.life);
        float fade = 1.f - t;
        float spread = e.fragmented ? 5.f : 3.f;
        float ring_radius = e.radius * (1.f + spread * t);

        sf::Color ring_color = e.color;
        ring_color.a = static_cast<std::uint8_t>(255.f * fade);

        sf::CircleShape ring(ring_radius);
        ring.setOrigin({ring_radius, ring_radius});
        ring.setPosition(e.position);
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineThickness(line_width);
        ring.setOutlineColor(ring_color);
        window.draw(ring);

        if(t < 0.3f) {
            float flash_fade = 1.f - t / 0.3f;
            float flash_radius = e.radius * (1.f + 2.f * t);

            sf::CircleShape flash(flash_radius);
            flash.setOrigin({flash_radius, flash_radius});
            flash.setPosition(e.position);
            flash.setFillColor(sf::Color(255, 255, 255,
                static_cast<std::uint8_t>(200.f * flash_fade)));
            window.draw(flash);
        }

        if(e.fragmented) {
            constexpr int SPARKS = 8;
            constexpr float PI = 3.14159265f;

            float base_angle = e.position.x * 0.37f + e.position.y * 0.11f;

            sf::Color spark_color = sf::Color(255, 220, 120);
            spark_color.a = static_cast<std::uint8_t>(255.f * fade);

            sf::VertexArray sparks(sf::PrimitiveType::Lines, SPARKS * 2);
            for(int k = 0; k < SPARKS; k++) {
                float angle = base_angle + 2.f * PI * k / SPARKS;
                sf::Vector2f dir(std::cos(angle), std::sin(angle));

                sparks[k * 2].position = e.position + dir * (ring_radius * 0.6f);
                sparks[k * 2].color = spark_color;
                sparks[k * 2 + 1].position = e.position + dir * (ring_radius * 1.3f);
                sparks[k * 2 + 1].color = spark_color;
            }
            window.draw(sparks);
        }

        e.age++;
    }

    effects.erase(
        std::remove_if(effects.begin(), effects.end(),
            [](const Effect& e) { return e.age >= e.life; }),
        effects.end()
    );
}

void Renderer::draw_launch_preview(const SimControl& control) {
    const Config::SpawnPreset& preset = Config::SPAWN_PRESETS[control.spawn_slot - 1];

    sf::Vector2f current = window.mapPixelToCoords(launch_pixel, view);

    const float size = static_cast<float>(
        Particle(preset.mass, preset.type, Vector2(), Vector2()).get_radius()
    );

    sf::Color color = color_of(preset.type);
    color.a = 160;

    sf::CircleShape circle(size);
    circle.setOrigin({size, size});
    circle.setFillColor(color);
    circle.setPosition(launch_start);
    window.draw(circle);

    const sf::Vertex line[2] = {
        sf::Vertex{launch_start, sf::Color::Yellow},
        sf::Vertex{current, sf::Color::Yellow}
    };
    window.draw(line, 2, sf::PrimitiveType::Lines);
}

void Renderer::draw_hud(const PhysicsWorld& world, const SimControl& control) {
    if(!hud_text) return;

    update_fps();

    window.setView(sf::View(sf::FloatRect(
        {0.f, 0.f},
        sf::Vector2f(window.getSize())
    )));

    std::string text =
        "[" + control.scenario_name + "]   " +
        "Bodies: " + std::to_string(world.get_particles().size()) +
        "   Speed: x" + std::to_string(control.time_scale) +
        "   FPS: " + std::to_string(static_cast<int>(fps + 0.5f));

    if(control.paused) text += "   [PAUSED]";

    const Config::SpawnPreset& preset = Config::SPAWN_PRESETS[control.spawn_slot - 1];
    text += "\nSpawn [" + std::to_string(control.spawn_slot) + "]: " +
            preset.name + " (mass " + std::to_string(static_cast<int>(preset.mass)) + ")";

    text += "\nF1-F5: scenario   1-4: body type   LMB drag: launch   RMB drag: pan   Wheel: zoom";
    text += "\nSpace: pause   +/-: speed   R: reset   T: trails   Esc: cancel   Tab: mode";

    hud_text->setString(text);
    window.draw(*hud_text);

    draw_mode_buttons(control);

    window.setView(view);
}

void Renderer::draw_bodies(const PhysicsWorld& world) {
    for(const Particle& p : world.get_particles()) {
        const float size = static_cast<float>(p.get_radius());

        sf::CircleShape circle(size);
        circle.setOrigin({size, size});
        circle.setFillColor(color_of(p.get_type()));
        circle.setPosition({
            static_cast<float>(p.get_position().x),
            static_cast<float>(p.get_position().y)
        });

        window.draw(circle);
    }
}

void Renderer::draw(const PhysicsWorld& world, const SimControl& control) {
    window.clear();

    window.setView(view);

    if(show_trails) {
        update_trails(world, trails);
        draw_trails(world, trails);
    }

    draw_bodies(world);

    draw_effects();

    if(launching) draw_launch_preview(control);

    draw_hud(world, control);

    window.display();
}

void Renderer::update_fps() {
    float dt = fps_clock.restart().asSeconds();
    if(dt > 0.f) fps = fps * 0.9f + (1.f / dt) * 0.1f;
}

void Renderer::draw_text(const std::string& text, sf::Vector2f position, sf::Color color,
                         unsigned size, bool right_align) {
    if(!hud_text) return;

    sf::Text label(font, text, size);
    label.setFillColor(color);

    if(right_align) position.x -= label.getLocalBounds().size.x;
    label.setPosition(position);

    window.draw(label);
}

sf::FloatRect Renderer::mode_button(int index) const {
    constexpr float WIDTH = 120.f;
    constexpr float HEIGHT = 26.f;
    constexpr float GAP = 6.f;

    const float right = static_cast<float>(window.getSize().x) - 10.f;
    const float x = right - (2 - index) * WIDTH - (1 - index) * GAP;

    return sf::FloatRect({x, 8.f}, {WIDTH, HEIGHT});
}

void Renderer::draw_mode_buttons(const SimControl& control) {
    const char* names[2] = {"Simulation", "Compare"};

    for(int i = 0; i < 2; i++) {
        const sf::FloatRect rect = mode_button(i);
        const bool active = (control.mode == Mode::SIMULATION) == (i == 0);

        sf::RectangleShape box(rect.size);
        box.setPosition(rect.position);
        box.setFillColor(active ? sf::Color(50, 110, 70) : sf::Color(30, 30, 30));
        box.setOutlineThickness(1.f);
        box.setOutlineColor(active ? sf::Color(120, 220, 150) : sf::Color(90, 90, 90));
        window.draw(box);

        if(!hud_text) continue;

        sf::Text label(font, names[i], 14);
        label.setFillColor(active ? sf::Color::White : sf::Color(160, 160, 160));

        const sf::FloatRect bounds = label.getLocalBounds();
        label.setPosition({
            rect.position.x + (rect.size.x - bounds.size.x) * 0.5f - bounds.position.x,
            rect.position.y + 4.f
        });
        window.draw(label);
    }
}

namespace {
    const sf::Color EULER_COLOR(255, 150, 50);
    const sf::Color VERLET_COLOR(90, 220, 140);

    std::string format_percent(double relative) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%+.4g%%", relative * 100.0);
        return buf;
    }
}

void Renderer::draw_compare(const PhysicsWorld& euler, const PhysicsWorld& verlet,
                            const EnergyLog& log, const SimControl& control) {
    window.clear();

    const sf::Vector2f win(window.getSize());
    const float panel_ratio = 1.f - Config::COMPARE_GRAPH_HEIGHT_RATIO;
    const PhysicsWorld* worlds[2] = {&euler, &verlet};

    for(int i = 0; i < 2; i++) {
        sf::View panel(view.getCenter(), {win.x * 0.5f / scale, win.y * panel_ratio / scale});
        panel.setViewport(sf::FloatRect({0.5f * i, 0.f}, {0.5f, panel_ratio}));
        window.setView(panel);

        if(show_trails) {
            update_trails(*worlds[i], compare_trails[i]);
            draw_trails(*worlds[i], compare_trails[i]);
        }

        draw_bodies(*worlds[i]);
    }

    window.setView(sf::View(sf::FloatRect({0.f, 0.f}, win)));
    draw_compare_overlay(log, control);

    window.setView(view);
    window.display();
}

void Renderer::draw_compare_overlay(const EnergyLog& log, const SimControl& control) {
    const sf::Vector2f win(window.getSize());
    const float panel_bottom = win.y * (1.f - Config::COMPARE_GRAPH_HEIGHT_RATIO);

    update_fps();

    const sf::Color border(80, 80, 80);
    const sf::Vertex lines[4] = {
        sf::Vertex{{win.x * 0.5f, 0.f}, border},
        sf::Vertex{{win.x * 0.5f, panel_bottom}, border},
        sf::Vertex{{0.f, panel_bottom}, border},
        sf::Vertex{{win.x, panel_bottom}, border}
    };
    window.draw(lines, 4, sf::PrimitiveType::Lines);

    const EnergySample& now = log.get_latest();

    char dt_text[16];
    std::snprintf(dt_text, sizeof(dt_text), "%g", Config::COMPARE_DT);

    std::string head =
        "[" + control.compare_scenario_name + "]   " +
        "dt: " + dt_text +
        "   Speed: x" + std::to_string(control.compare_time_scale) +
        "   t: " + std::to_string(static_cast<int>(now.time)) +
        "   FPS: " + std::to_string(static_cast<int>(fps + 0.5f));

    if(control.paused) head += "   [PAUSED]";

    head += "\nF1-F4: scenario   RMB drag: pan   Wheel: zoom   Space: pause   +/-: speed";
    head += "\nR: reset   T: trails   L: log graph   S: save CSV   Tab: mode";

    if(hud_text) {
        hud_text->setString(head);
        window.draw(*hud_text);
    }

    draw_text("EULER   dE/|E0| = " + format_percent(now.euler),
              {10.f, 62.f}, EULER_COLOR, 16);
    draw_text("VERLET  dE/|E0| = " + format_percent(now.verlet),
              {win.x * 0.5f + 10.f, 62.f}, VERLET_COLOR, 16);

    draw_mode_buttons(control);

    if(control.status_frames > 0) {
        draw_text(control.status_message, {win.x - 10.f, 40.f}, sf::Color(255, 230, 120), 14, true);
    }

    draw_energy_graph(log, control,
        sf::FloatRect({70.f, panel_bottom + 34.f}, {win.x - 90.f, win.y - panel_bottom - 64.f}));
}

void Renderer::draw_energy_graph(const EnergyLog& log, const SimControl& control,
                                 sf::FloatRect area) {
    const auto& samples = log.get_samples();
    const bool use_log = control.log_graph;

    draw_text(use_log ? "Total energy error  log10 |E - E0| / |E0|"
                      : "Total energy error  (E - E0) / |E0|  [%]",
              {area.position.x, area.position.y - 24.f}, sf::Color(200, 200, 200));
    draw_text("Euler", {area.position.x + area.size.x - 90.f, area.position.y - 24.f},
              EULER_COLOR, 14, true);
    draw_text("Verlet", {area.position.x + area.size.x, area.position.y - 24.f},
              VERLET_COLOR, 14, true);

    sf::RectangleShape frame(area.size);
    frame.setPosition(area.position);
    frame.setFillColor(sf::Color(18, 18, 18));
    frame.setOutlineThickness(1.f);
    frame.setOutlineColor(sf::Color(100, 100, 100));
    window.draw(frame);

    if(samples.size() < 2) return;

    auto transform = [&](double relative) {
        if(use_log) return std::log10(std::max(std::abs(relative), 1e-12));
        return relative * 100.0;
    };

    double y_min = transform(samples.front().euler);
    double y_max = y_min;

    for(const EnergySample& s : samples) {
        for(double v : {transform(s.euler), transform(s.verlet)}) {
            y_min = std::min(y_min, v);
            y_max = std::max(y_max, v);
        }
    }

    std::vector<std::pair<double, std::string>> ticks;
    char buf[32];

    if(use_log) {
        y_min = std::floor(y_min);
        y_max = std::ceil(y_max);
        if(y_max - y_min < 1.0) y_max = y_min + 1.0;

        const int step = std::max(1, static_cast<int>(std::ceil((y_max - y_min) / 5.0)));
        for(double d = y_min; d <= y_max + 1e-9; d += step) {
            std::snprintf(buf, sizeof(buf), "1e%+d", static_cast<int>(d));
            ticks.emplace_back(d, buf);
        }
    }
    else {
        y_min = std::min(y_min, 0.0);
        y_max = std::max(y_max, 0.0);
        const double pad = std::max((y_max - y_min) * 0.1, 1e-9);
        y_min -= pad;
        y_max += pad;

        for(int k = 0; k <= 4; k++) {
            const double v = y_min + (y_max - y_min) * k / 4.0;
            std::snprintf(buf, sizeof(buf), "%+.3g", v);
            ticks.emplace_back(v, buf);
        }
    }

    const double t_max = std::max(samples.back().time, 1e-9);

    auto to_screen = [&](double t, double y) {
        return sf::Vector2f(
            area.position.x + static_cast<float>(t / t_max) * area.size.x,
            area.position.y + area.size.y -
                static_cast<float>((y - y_min) / (y_max - y_min)) * area.size.y
        );
    };

    for(const auto& tick : ticks) {
        const sf::Vector2f left = to_screen(0.0, tick.first);

        const sf::Color grid(60, 60, 60);
        const sf::Vertex line[2] = {
            sf::Vertex{left, grid},
            sf::Vertex{{left.x + area.size.x, left.y}, grid}
        };
        window.draw(line, 2, sf::PrimitiveType::Lines);

        draw_text(tick.second, {area.position.x - 6.f, left.y - 9.f},
                  sf::Color(160, 160, 160), 12, true);
    }

    if(!use_log) {
        const sf::Vector2f zero = to_screen(0.0, 0.0);
        const sf::Color c(140, 140, 140);
        const sf::Vertex line[2] = {
            sf::Vertex{zero, c},
            sf::Vertex{{zero.x + area.size.x, zero.y}, c}
        };
        window.draw(line, 2, sf::PrimitiveType::Lines);
    }

    draw_text("t = 0", {area.position.x, area.position.y + area.size.y + 4.f},
              sf::Color(160, 160, 160), 12);
    std::snprintf(buf, sizeof(buf), "t = %.1f", t_max);
    draw_text(buf, {area.position.x + area.size.x, area.position.y + area.size.y + 4.f},
              sf::Color(160, 160, 160), 12, true);

    sf::VertexArray euler_line(sf::PrimitiveType::LineStrip, samples.size());
    sf::VertexArray verlet_line(sf::PrimitiveType::LineStrip, samples.size());

    for(std::size_t i = 0; i < samples.size(); i++) {
        euler_line[i].position = to_screen(samples[i].time, transform(samples[i].euler));
        euler_line[i].color = EULER_COLOR;
        verlet_line[i].position = to_screen(samples[i].time, transform(samples[i].verlet));
        verlet_line[i].color = VERLET_COLOR;
    }

    window.draw(euler_line);
    window.draw(verlet_line);
}
