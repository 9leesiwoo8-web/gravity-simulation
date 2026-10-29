#include "../Config/Config.h"
#include "../Physics/PhysicsWorld.h"
#include "../Renderer/Renderer.h"
#include "../Scenario/Scenario.h"

// 선택된 시나리오로 world를 새로 만든다.
static void load_scenario(PhysicsWorld& world, Renderer& renderer, SimControl& control)
{
    const std::vector<Scenario>& list = scenarios();

    // 없는 번호의 F 키를 눌렀다면 첫 번째 시나리오로 돌아간다.
    if(control.scenario < 0 || control.scenario >= static_cast<int>(list.size()))
    {
        control.scenario = 0;
    }

    const Scenario& scenario = list[control.scenario];

    world = PhysicsWorld(Config::G);
    scenario.build(world);

    renderer.clear_trails();
    renderer.clear_effects();

    control.scenario_name = scenario.name;
    control.reset_requested = false;
}

int main()
{
    PhysicsWorld world(Config::G);

    Renderer renderer;
    SimControl control;

    load_scenario(world, renderer, control);

    while(renderer.is_open())
    {
        renderer.process_events(control);

        if(control.reset_requested)
        {
            load_scenario(world, renderer, control);
        }

        if(control.spawn_requested)
        {
            const Config::SpawnPreset& preset = Config::SPAWN_PRESETS[control.spawn_slot - 1];

            world.add_Particle(
                Particle(
                    preset.mass,
                    preset.type,
                    control.spawn_position,
                    control.spawn_velocity
                )
            );

            control.spawn_requested = false;
        }

        if(!control.paused)
        {
            world.update(Config::DT, control.time_scale);
        }

        for(const CollisionEvent& event : world.take_events())
        {
            renderer.add_effect(event);
        }

        renderer.draw(world, control);
    }

    return 0;
}
