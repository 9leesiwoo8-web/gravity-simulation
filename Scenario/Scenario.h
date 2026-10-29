#pragma once

#include "../Physics/PhysicsWorld.h"

#include <vector>

// 시작 배치 하나. build는 비어 있는 world에 천체를 채운다.
struct Scenario {
    const char* name;
    void (*build)(PhysicsWorld& world);
};

// F1부터 순서대로 고를 수 있는 시나리오 목록
const std::vector<Scenario>& scenarios();

// 비교 모드용 시나리오. 난수 없이 항상 같은 배치를 만들고, 천체가 적어 CPU로 충분하다.
const std::vector<Scenario>& compare_scenarios();
