#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct EnergySample {
    double time;
    double euler;
    double verlet;
    double euler_energy;
    double verlet_energy;
};

class EnergyLog {
private:
    std::vector<EnergySample> samples;
    EnergySample latest{};

    double euler_initial = 0.0;
    double verlet_initial = 0.0;

    std::size_t stride = 1;
    std::size_t counter = 0;

public:
    void reset();

    void record(double time, double euler_energy, double verlet_energy);

    bool save_csv(const std::string& path) const;

    const std::vector<EnergySample>& get_samples() const { return samples; }
    const EnergySample& get_latest() const { return latest; }
};
