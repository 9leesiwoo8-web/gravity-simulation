#include "Renderer.h"

#include <algorithm>
#include <cmath>
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

// 마우스 아래의 월드 좌표가 줌 전후로 같은 위치에 오도록 한다.
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

    // F1~F9: 시나리오 선택. 유효한 번호인지는 main이 확인한다.
    const int function_key = static_cast<int>(code) - static_cast<int>(Key::F1);
    if(function_key >= 0 && function_key < 9) {
        control.scenario = function_key;
        control.reset_requested = true;
        return;
    }

    switch(code) {
    case Key::Space:
        control.paused = !control.paused;
        break;
    case Key::Equal:
    case Key::Add:
        control.time_scale = std::min(control.time_scale * 2, Config::MAX_TIME_SCALE);
        break;
    case Key::Hyphen:
    case Key::Subtract:
        control.time_scale = std::max(control.time_scale / 2, Config::MIN_TIME_SCALE);
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
            zoom_at(scroll->delta > 0 ? Config::ZOOM_IN_FACTOR : Config::ZOOM_OUT_FACTOR,
                    scroll->position);
        }

        if(const auto* pressed = event->getIf<sf::Event::MouseButtonPressed>()) {
            if(pressed->button == sf::Mouse::Button::Left) {
                launching = true;
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

void Renderer::update_trails(const PhysicsWorld& world) {
    std::unordered_set<std::uint64_t> alive;

    for(const Particle& p : world.get_particles()) {
        alive.insert(p.get_id());

        sf::Vector2f pos(
            static_cast<float>(p.get_position().x),
            static_cast<float>(p.get_position().y)
        );

        auto& trail = trails[p.get_id()];

        // 일시정지 중에는 같은 점이 쌓이지 않도록 한다.
        if(!trail.empty() && trail.back() == pos) continue;

        trail.push_back(pos);
        if(trail.size() > Config::TRAIL_LENGTH) trail.pop_front();
    }

    for(auto it = trails.begin(); it != trails.end();) {
        if(alive.count(it->first) == 0) it = trails.erase(it);
        else ++it;
    }
}

void Renderer::draw_trails(const PhysicsWorld& world) {
    for(const Particle& p : world.get_particles()) {
        auto it = trails.find(p.get_id());
        if(it == trails.end() || it->second.size() < 2) continue;

        const auto& trail = it->second;
        sf::Color base = color_of(p.get_type());

        sf::VertexArray line(sf::PrimitiveType::LineStrip, trail.size());
        for(std::size_t i = 0; i < trail.size(); i++) {
            // 오래된 점일수록 투명하게
            float t = static_cast<float>(i) / static_cast<float>(trail.size() - 1);
            sf::Color c = base;
            c.a = static_cast<std::uint8_t>(10 + 110 * t);

            line[i].position = trail[i];
            line[i].color = c;
        }

        window.draw(line);
    }
}

// 충돌 이펙트: 퍼져 나가며 사라지는 링. 파편화는 섬광과 불꽃이 더해진다.
void Renderer::draw_effects() {
    const float line_width = 2.f / scale;   // 줌과 상관없이 화면에서 일정한 굵기

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

        // 충돌 직후의 짧은 섬광
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

        // 파편화: 사방으로 튀는 불꽃
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

// 발사 전 조준선: 시작점에 생성될 천체와 드래그 방향 선
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

    float dt = fps_clock.restart().asSeconds();
    if(dt > 0.f) fps = fps * 0.9f + (1.f / dt) * 0.1f;

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
    text += "\nSpace: pause   +/-: speed   R: reset   T: trails   Esc: cancel";

    hud_text->setString(text);
    window.draw(*hud_text);

    window.setView(view);
}

void Renderer::draw(const PhysicsWorld& world, const SimControl& control) {
    window.clear();

    window.setView(view);

    if(show_trails) {
        update_trails(world);
        draw_trails(world);
    }

    for(const Particle& p : world.get_particles()) {
        // 반지름은 월드 단위. 줌은 view가 처리한다.
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

    draw_effects();

    if(launching) draw_launch_preview(control);

    draw_hud(world, control);

    window.display();
}
