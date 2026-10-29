#include "EnergyLog.h"

#include "../Config/Config.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
    double relative_error(double energy, double initial) {
        double scale = std::max(std::abs(initial), 1e-300);
        return (energy - initial) / scale;
    }
}

void EnergyLog::reset() {
    samples.clear();
    latest = {};
    stride = 1;
    counter = 0;
}

void EnergyLog::record(double time, double euler_energy, double verlet_energy) {
    if(samples.empty()) {
        euler_initial = euler_energy;
        verlet_initial = verlet_energy;
        counter = 0;
    }

    latest = {
        time,
        relative_error(euler_energy, euler_initial),
        relative_error(verlet_energy, verlet_initial),
        euler_energy,
        verlet_energy
    };

    if(counter++ % stride != 0) return;

    samples.push_back(latest);

    if(samples.size() >= Config::ENERGY_LOG_MAX_SAMPLES) {
        std::size_t kept = 0;
        for(std::size_t i = 0; i < samples.size(); i += 2) {
            samples[kept++] = samples[i];
        }
        samples.resize(kept);
        stride *= 2;
        counter = 0;
    }
}

bool EnergyLog::save_csv(const std::string& path) const {
    std::FILE* file = std::fopen(path.c_str(), "w");
    if(!file) return false;

    std::fprintf(file, "time,euler_error,verlet_error,euler_energy,verlet_energy\n");

    for(const EnergySample& s : samples) {
        std::fprintf(file, "%.10g,%.10e,%.10e,%.10g,%.10g\n",
                     s.time, s.euler, s.verlet, s.euler_energy, s.verlet_energy);
    }

    return std::fclose(file) == 0;
}
