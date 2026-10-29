#pragma once

// 프로젝트 전체의 조절 가능한 값을 한곳에 모은 파일.
// 값을 바꾸면 해당 값을 쓰는 모든 코드에 반영된다.

#include "../Physics/Particle.h"

#include <cstddef>

namespace Config {

    // ---- 물리 ----
    inline constexpr double G = 100.0;
    inline constexpr double SOFTENING = 5.0;            // 중력 softening 길이
    inline constexpr double DT = 0.001;                 // 물리 스텝 시간

    // 입자가 이 수 이상일 때만 GPU로 계산한다. 그보다 적으면 CPU가 더 빠르다.
    // (100스텝에 CPU는 입자 100개에서 약 2.6ms, 160개에서 약 6ms, GPU는 500개 이하에서 약 2.4ms)
    inline constexpr std::size_t GPU_MIN_PARTICLES = 128;

    // GPU는 입자가 처음 겹친 뒤 이 스텝 수만큼 더 진행하고 멈춰서 충돌을 처리한다.
    // 클수록 CPU와 주고받는 횟수가 줄어 빠르지만 겹침이 깊어진 뒤 병합된다 (0이면 겹친 스텝에서 바로).
    inline constexpr int GPU_COLLISION_DELAY_STEPS = 32;

    // 천체 반지름 = RADIUS_BASE + RADIUS_PER_CBRT_MASS * cbrt(질량)
    inline constexpr double RADIUS_BASE = 2.0;
    inline constexpr double RADIUS_PER_CBRT_MASS = 0.8;

    // ---- 비교 모드 (오일러 vs Verlet 에너지 비교) ----
    // 오차가 눈에 보이도록 시뮬레이션 모드보다 큰 스텝을 쓴다.
    inline constexpr double COMPARE_DT = 0.01;
    inline constexpr int COMPARE_DEFAULT_TIME_SCALE = 20;   // 프레임당 물리 스텝 수
    inline constexpr std::size_t ENERGY_LOG_MAX_SAMPLES = 2000;
    inline constexpr int STATUS_MESSAGE_FRAMES = 240;       // 저장 안내를 보여 주는 시간
    inline constexpr float COMPARE_GRAPH_HEIGHT_RATIO = 0.38f;  // 창 높이 중 그래프 비율

    // ---- 궤도 이탈 ----
    // 질량중심에서 이 거리보다 멀어지면 제거한다.
    inline constexpr double ESCAPE_RADIUS = 4000.0;

    // ---- 파편화 ----
    inline constexpr int FRAGMENT_COUNT = 4;
    inline constexpr double FRAGMENT_SPEED_FACTOR = 1.0;     // 충돌 속도 / 탈출 속도
    inline constexpr double FRAGMENT_MAX_MASS_RATIO = 10.0;  // 이보다 크게 차이나면 큰 쪽이 흡수
    inline constexpr double MIN_FRAGMENT_MASS = 0.3;         // 파편 한 조각의 최소 질량
    inline constexpr double FRAGMENT_SPREAD_FACTOR = 0.3;    // 충돌 속도 대비 파편이 퍼지는 속도

    // ---- 시간 흐름 ----
    inline constexpr int DEFAULT_TIME_SCALE = 100;      // 프레임당 물리 스텝 수
    inline constexpr int MIN_TIME_SCALE = 1;
    inline constexpr int MAX_TIME_SCALE = 1600;

    // ---- 창과 카메라 ----
    inline constexpr unsigned WINDOW_WIDTH = 1200;
    inline constexpr unsigned WINDOW_HEIGHT = 800;
    inline constexpr unsigned FRAMERATE_LIMIT = 60;
    inline constexpr float DEFAULT_SCALE = 0.6f;
    inline constexpr float MIN_SCALE = 0.1f;
    inline constexpr float MAX_SCALE = 5.0f;
    inline constexpr float ZOOM_IN_FACTOR = 1.1f;
    inline constexpr float ZOOM_OUT_FACTOR = 0.9f;

    // ---- 화면 효과 ----
    inline constexpr std::size_t TRAIL_LENGTH = 300;
    inline constexpr std::size_t MAX_EFFECTS = 500;
    inline constexpr int MERGE_EFFECT_LIFE = 30;        // 프레임
    inline constexpr int FRAGMENT_EFFECT_LIFE = 50;     // 프레임

    // ---- 조작 ----
    // 드래그한 월드 거리 1당 초기 속력
    inline constexpr double LAUNCH_SPEED_FACTOR = 0.5;

    // ---- 사용자가 만드는 천체 (숫자 키 1~4) ----
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
