#pragma once

#include "Particle.h"

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

class GpuGravity {
public:
    GpuGravity();
    ~GpuGravity();

    GpuGravity(const GpuGravity&) = delete;
    GpuGravity& operator=(const GpuGravity&) = delete;

    bool available() const { return ready.load(std::memory_order_acquire); }

    bool advance(std::vector<Particle>& particles, double dt, int steps, double G, double eps_sq,
                 bool refresh_acceleration, int collision_delay,
                 int& steps_done, bool& collision);

private:
    struct Impl;
    Impl* impl = nullptr;
    std::atomic<bool> ready{false};
    std::thread init_thread;

    void init();
    void release();
    bool ensure_capacity(std::size_t padded);
};
