#pragma once

#include "../Physics/Particle.h"

#include <cstddef>

namespace Config {

    inline constexpr double G = 100.0;
    inline constexpr double SOFTENING = 5.0;
    inline constexpr double DT = 0.001;

    inline constexpr std::size_t GPU_MIN_PARTICLES = 128;

    inline constexpr int GPU_COLLISION_DELAY_STEPS = 32;

    inline constexpr double RADIUS_BASE = 2.0;
    inline constexpr double RADIUS_PER_CBRT_MASS = 0.8;

    inline constexpr double COMPARE_DT = 0.01;
    inline constexpr int COMPARE_DEFAULT_TIME_SCALE = 20;
    inline constexpr std::size_t ENERGY_LOG_MAX_SAMPLES = 2000;
    inline constexpr int STATUS_MESSAGE_FRAMES = 240;
    inline constexpr float COMPARE_GRAPH_HEIGHT_RATIO = 0.38f;

    inline constexpr double ESCAPE_RADIUS = 4000.0;

    inline constexpr int FRAGMENT_COUNT = 4;
    inline constexpr double FRAGMENT_SPEED_FACTOR = 1.0;
    inline constexpr double FRAGMENT_MAX_MASS_RATIO = 10.0;
    inline constexpr double MIN_FRAGMENT_MASS = 0.3;
    inline constexpr double FRAGMENT_SPREAD_FACTOR = 0.3;

    inline constexpr int DEFAULT_TIME_SCALE = 100;
    inline constexpr int MIN_TIME_SCALE = 1;
    inline constexpr int MAX_TIME_SCALE = 1600;

    inline constexpr unsigned WINDOW_WIDTH = 1200;
    inline constexpr unsigned WINDOW_HEIGHT = 800;
    inline constexpr unsigned FRAMERATE_LIMIT = 60;
    inline constexpr float DEFAULT_SCALE = 0.6f;
    inline constexpr float MIN_SCALE = 0.1f;
    inline constexpr float MAX_SCALE = 5.0f;
    inline constexpr float ZOOM_IN_FACTOR = 1.1f;
    inline constexpr float ZOOM_OUT_FACTOR = 0.9f;

    inline constexpr std::size_t TRAIL_LENGTH = 300;
    inline constexpr std::size_t MAX_EFFECTS = 500;
    inline constexpr int MERGE_EFFECT_LIFE = 30;
    inline constexpr int FRAGMENT_EFFECT_LIFE = 50;

    inline constexpr double LAUNCH_SPEED_FACTOR = 0.5;

    struct SpawnPreset {
        Type type;
        double mass;
        const char* name;
    };

    inline constexpr SpawnPreset SPAWN_PRESETS[4] = {
        {Type::ASTEROID,    1.0, "ASTEROID"},
        {Type::PLANET,     10.0, "PLANET"},
        {Type::GIANT,     200.0, "GIANT"},
        {Type::SUN,      2000.0, "STAR"}
    };
    inline constexpr int DEFAULT_SPAWN_SLOT = 2;

}
