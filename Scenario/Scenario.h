#pragma once

#include "../Physics/PhysicsWorld.h"

#include <vector>

struct Scenario {
    const char* name;
    void (*build)(PhysicsWorld& world);
};

const std::vector<Scenario>& scenarios();

const std::vector<Scenario>& compare_scenarios();
