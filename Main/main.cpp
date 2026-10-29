#include "../Config/Config.h"
#include "../Physics/EnergyLog.h"
#include "../Physics/PhysicsWorld.h"
#include "../Renderer/Renderer.h"
#include "../Scenario/Scenario.h"

#include <cctype>
#include <ctime>
#include <string>

static void load_scenario(PhysicsWorld& world, Renderer& renderer, SimControl& control)
{
    const std::vector<Scenario>& list = scenarios();

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

static void load_compare(PhysicsWorld& euler, PhysicsWorld& verlet, EnergyLog& log,
                         double& time, Renderer& renderer, SimControl& control)
{
    const std::vector<Scenario>& list = compare_scenarios();

    if(control.compare_scenario < 0 || control.compare_scenario >= static_cast<int>(list.size()))
    {
        control.compare_scenario = 0;
    }

    const Scenario& scenario = list[control.compare_scenario];

    euler = PhysicsWorld(Config::G);
    euler.set_integrator(Integrator::EULER);
    euler.set_closed_system(true);
    euler.set_gpu_allowed(false);
    scenario.build(euler);

    verlet = PhysicsWorld(Config::G);
    verlet.set_integrator(Integrator::VERLET);
    verlet.set_closed_system(true);
    verlet.set_gpu_allowed(false);
    scenario.build(verlet);

    log.reset();
    time = 0.0;
    log.record(time, euler.total_energy(), verlet.total_energy());

    renderer.clear_compare_trails();

    control.compare_scenario_name = scenario.name;
    control.reset_requested = false;
}

static std::string csv_file_name(const std::string& scenario_name)
{
    std::string name;
    for(char c : scenario_name)
    {
        name += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
    }

    char stamp[32];
    std::time_t now = std::time(nullptr);
    std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", std::localtime(&now));

    return "energy_" + name + "_" + stamp + ".csv";
}

int main()
{
    PhysicsWorld world(Config::G);

    PhysicsWorld euler_world(Config::G);
    PhysicsWorld verlet_world(Config::G);
    EnergyLog energy_log;
    double compare_time = 0.0;
    bool compare_loaded = false;

    Renderer renderer;
    SimControl control;

    load_scenario(world, renderer, control);

    while(renderer.is_open())
    {
        renderer.process_events(control);

        if(control.mode_changed)
        {
            control.mode_changed = false;

            if(control.mode == Mode::COMPARE && !compare_loaded)
            {
                load_compare(euler_world, verlet_world, energy_log, compare_time, renderer, control);
                compare_loaded = true;
            }
        }

        if(control.reset_requested)
        {
            if(control.mode == Mode::COMPARE)
            {
                load_compare(euler_world, verlet_world, energy_log, compare_time, renderer, control);
                compare_loaded = true;
            }
            else
            {
                load_scenario(world, renderer, control);
            }
        }

        if(control.mode == Mode::COMPARE)
        {
            if(control.save_requested)
            {
                control.save_requested = false;

                const std::string path = csv_file_name(control.compare_scenario_name);
                control.status_message = energy_log.save_csv(path)
                    ? "Saved: " + path
                    : "Save failed: " + path;
                control.status_frames = Config::STATUS_MESSAGE_FRAMES;
            }

            if(control.status_frames > 0) control.status_frames--;

            if(!control.paused)
            {
                euler_world.update(Config::COMPARE_DT, control.compare_time_scale);
                verlet_world.update(Config::COMPARE_DT, control.compare_time_scale);

                compare_time += Config::COMPARE_DT * control.compare_time_scale;
                energy_log.record(compare_time, euler_world.total_energy(), verlet_world.total_energy());
            }

            renderer.draw_compare(euler_world, verlet_world, energy_log, control);
            continue;
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
