#pragma once

#include <cstddef>
#include <string>
#include <vector>

// 한 시점의 역학적 에너지 상대 오차 (E - E0) / |E0|와 에너지 자체
struct EnergySample {
    double time;
    double euler;
    double verlet;
    double euler_energy;
    double verlet_energy;
};

// 비교 모드에서 두 적분 방식의 에너지 변화를 시간 순으로 모은다.
// 가득 차면 절반으로 솎아내고 기록 간격을 두 배로 늘려, 전체 기간이 항상 그래프에 들어간다.
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

    // 첫 호출의 에너지가 기준(E0)이 된다.
    void record(double time, double euler_energy, double verlet_energy);

    // time, 두 방식의 상대 오차와 에너지를 한 줄씩 CSV로 쓴다. 성공하면 true.
    // 기록이 가득 차 솎아낸 적이 있으면 그만큼 간격이 성기다.
    bool save_csv(const std::string& path) const;

    const std::vector<EnergySample>& get_samples() const { return samples; }
    const EnergySample& get_latest() const { return latest; }
};
