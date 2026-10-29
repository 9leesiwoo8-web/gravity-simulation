#pragma once

#include "Particle.h"

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

// OpenCL로 N-body 중력(O(N^2)) 적분을 GPU에서 한다.
// 드라이버 초기화와 커널 컴파일은 느려서(1~2초) 백그라운드 스레드에서 한다.
// 준비가 끝나기 전이나 초기화에 실패하면(double 미지원 포함) available()이 false이며,
// 호출하는 쪽에서 CPU 경로로 계산한다.
//
// 힘 계산은 float, 위치/속도/가속도 상태와 적분은 double로 한다.
class GpuGravity {
public:
    GpuGravity();
    ~GpuGravity();

    GpuGravity(const GpuGravity&) = delete;
    GpuGravity& operator=(const GpuGravity&) = delete;

    bool available() const { return ready.load(std::memory_order_acquire); }

    // 속도 Verlet을 최대 steps번 GPU에서 연속으로 진행한다 (스텝마다 CPU와 주고받지 않는다).
    // refresh_acceleration이 true면 시작 전에 현재 위치에서 가속도를 새로 구한다
    // (입자가 추가/제거/병합되어 저장된 가속도가 낡았을 때). 아니면 particles의 가속도가
    // 현재 위치 기준으로 유효해야 한다.
    // 반지름이 겹친 쌍이 처음 생기면 collision_delay스텝만 더 진행하고 멈춘다 (0이면 그 스텝에서 바로).
    // 진행한 스텝 수를 steps_done에, 겹침이 있었는지를 collision에 돌려준다.
    // 멈춘 시점의 상태가 particles에 반영된다.
    // 실패하면 false를 반환하며 particles는 바뀌지 않는다. 이후로는 available()도 false가 된다.
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
