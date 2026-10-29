#include "GpuGravity.h"

#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>

#include <cstdio>
#include <string>

namespace {

    constexpr std::size_t TILE = 64;
    constexpr std::size_t SLICES = 4;
    constexpr std::size_t GROUP_SIZE = TILE * SLICES;

    const char* KERNEL_SOURCE = R"CLC(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

#define TILE 64
#define SLICES 4
#define GROUP (TILE * SLICES)

typedef struct {
    double px, py;
    double vx, vy;
    double ax, ay;
    double mass;
    double pad;
} State;

float4 sum_forces(__global const float4* body, __local float4* tile, __local float4* red,
                  int i, int padded, float G, float eps_sq)
{
    int lid = get_local_id(0);
    int slice = lid / TILE;

    float4 a = body[i];
    float fx = 0.0f;
    float fy = 0.0f;
    float hit = 0.0f;

    for(int base = 0; base < padded; base += GROUP) {
        tile[lid] = body[base + lid];
        barrier(CLK_LOCAL_MEM_FENCE);

        for(int k = 0; k < TILE; k++) {
            float4 b = tile[slice * TILE + k];
            float dx = b.x - a.x;
            float dy = b.y - a.y;
            float dist_sq = dx * dx + dy * dy;

            float touch = a.w + b.w;
            if(base + slice * TILE + k > i && dist_sq < touch * touch) hit = 1.0f;

            float inv = rsqrt(dist_sq + eps_sq);
            float factor = G * a.z * b.z * inv * inv * inv;

            fx += dx * factor;
            fy += dy * factor;
        }

        barrier(CLK_LOCAL_MEM_FENCE);
    }

    red[lid] = (float4)(fx, fy, hit, 0.0f);
    barrier(CLK_LOCAL_MEM_FENCE);

    int first = lid % TILE;
    float4 sum = red[first];
    for(int s = 1; s < SLICES; s++) sum += red[s * TILE + first];

    return sum;
}

__kernel __attribute__((reqd_work_group_size(GROUP, 1, 1)))
void refresh_acceleration(__global State* st,
                          __global const float4* body,
                          const int n,
                          const int padded,
                          const float G,
                          const float eps_sq)
{
    __local float4 tile[GROUP];
    __local float4 red[GROUP];

    int i = get_group_id(0) * TILE + get_local_id(0) % TILE;
    float4 r = sum_forces(body, tile, red, i, padded, G, eps_sq);

    if(get_local_id(0) < TILE && i < n) {
        double2 a = convert_double2(r.xy) / st[i].mass;
        st[i].ax = a.x;
        st[i].ay = a.y;
    }
}

__kernel void predict(__global const State* st,
                      __global double2* pos_next,
                      __global const float4* body_in,
                      __global float4* body_out,
                      const int n,
                      const double dt)
{
    int i = get_global_id(0);

    float4 b = body_in[i];

    if(i >= n) {
        body_out[i] = b;
        return;
    }

    State s = st[i];
    double2 p = (double2)(s.px, s.py) + (double2)(s.vx, s.vy) * dt
              + (double2)(s.ax, s.ay) * (0.5 * dt * dt);

    pos_next[i] = p;
    body_out[i] = (float4)((float)p.x, (float)p.y, b.z, b.w);
}

__kernel __attribute__((reqd_work_group_size(GROUP, 1, 1)))
void step(__global State* st,
          __global double2* pos_next,
          __global const float4* body_in,
          __global float4* body_out,
          __global int* ctrl,
          const int flag_in,
          const int flag_out,
          const int n,
          const int padded,
          const float G,
          const float eps_sq,
          const double dt,
          const int delay)
{
    int signal = ctrl[flag_in];
    if(signal == 0) {
        if(get_global_id(0) == 0) ctrl[flag_out] = 0;
        return;
    }

    __local float4 tile[GROUP];
    __local float4 red[GROUP];

    int i = get_group_id(0) * TILE + get_local_id(0) % TILE;
    bool lead = get_local_id(0) < TILE;
    if(i == 0 && lead) {
        ctrl[2] += 1;
        if(signal > 0) ctrl[flag_out] = signal - 1;
    }

    float4 r = sum_forces(body_in, tile, red, i, padded, G, eps_sq);

    if(lead && i < n) {
        State s = st[i];

        double2 p = pos_next[i];
        double2 new_acc = convert_double2(r.xy) / s.mass;

        double2 v = (double2)(s.vx, s.vy) + ((double2)(s.ax, s.ay) + new_acc) * (0.5 * dt);

        s.px = p.x;  s.py = p.y;
        s.vx = v.x;  s.vy = v.y;
        s.ax = new_acc.x;  s.ay = new_acc.y;
        st[i] = s;

        double2 pn = p + v * dt + new_acc * (0.5 * dt * dt);
        pos_next[i] = pn;

        float4 b = body_in[i];
        body_out[i] = (float4)((float)pn.x, (float)pn.y, b.z, b.w);
    }

    if(lead && r.z != 0.0f && signal < 0) ctrl[flag_out] = delay;
}
)CLC";

    struct Body {
        float x, y, mass, radius;
    };

    struct State {
        double px, py;
        double vx, vy;
        double ax, ay;
        double mass;
        double pad;
    };

    void log_error(const char* what, cl_int err) {
        std::fprintf(stderr, "[GpuGravity] %s 실패 (OpenCL 오류 %d)\n", what, err);
    }

    bool has_fp64(cl_device_id device) {
        size_t size = 0;
        if(clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, 0, nullptr, &size) != CL_SUCCESS) return false;

        std::string ext(size, '\0');
        if(clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, size, &ext[0], nullptr) != CL_SUCCESS) return false;

        return ext.find("cl_khr_fp64") != std::string::npos;
    }

    cl_device_id pick_device() {
        cl_uint platform_count = 0;
        if(clGetPlatformIDs(0, nullptr, &platform_count) != CL_SUCCESS || platform_count == 0) return nullptr;

        std::vector<cl_platform_id> platforms(platform_count);
        clGetPlatformIDs(platform_count, platforms.data(), nullptr);

        cl_device_id best = nullptr;
        bool best_discrete = false;

        for(cl_platform_id platform : platforms) {
            cl_uint device_count = 0;
            if(clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, nullptr, &device_count) != CL_SUCCESS) continue;
            if(device_count == 0) continue;

            std::vector<cl_device_id> devices(device_count);
            clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, device_count, devices.data(), nullptr);

            for(cl_device_id d : devices) {
                if(!has_fp64(d)) continue;

                cl_bool unified = CL_TRUE;
                clGetDeviceInfo(d, CL_DEVICE_HOST_UNIFIED_MEMORY, sizeof(unified), &unified, nullptr);

                bool discrete = (unified == CL_FALSE);
                if(!best || (discrete && !best_discrete)) {
                    best = d;
                    best_discrete = discrete;
                }
            }
        }

        return best;
    }

    void release_mem(cl_mem& mem) {
        if(mem) clReleaseMemObject(mem);
        mem = nullptr;
    }

    void release_kernel(cl_kernel& kernel) {
        if(kernel) clReleaseKernel(kernel);
        kernel = nullptr;
    }

}

struct GpuGravity::Impl {
    cl_context context = nullptr;
    cl_command_queue queue = nullptr;
    cl_program program = nullptr;
    cl_kernel refresh_kernel = nullptr;
    cl_kernel predict_kernel = nullptr;
    cl_kernel step_even = nullptr;
    cl_kernel step_odd = nullptr;

    cl_mem state_buf = nullptr;
    cl_mem pos_next_buf = nullptr;
    cl_mem body_a = nullptr;
    cl_mem body_b = nullptr;
    cl_mem ctrl_buf = nullptr;
    std::size_t capacity = 0;

    std::vector<Body> bodies;
    std::vector<State> states;
};

GpuGravity::GpuGravity() {
    init_thread = std::thread([this] { init(); });
}

GpuGravity::~GpuGravity() {
    if(init_thread.joinable()) init_thread.join();
    release();
}

void GpuGravity::init() {
    impl = new Impl;

    cl_device_id device = pick_device();
    if(!device) {
        std::fprintf(stderr, "[GpuGravity] double을 지원하는 GPU가 없어 CPU로 계산합니다.\n");
        release();
        return;
    }

    char name[256] = {};
    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(name) - 1, name, nullptr);

    cl_int err = CL_SUCCESS;
    impl->context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
    if(err != CL_SUCCESS) { log_error("clCreateContext", err); release(); return; }

    impl->queue = clCreateCommandQueue(impl->context, device, 0, &err);
    if(err != CL_SUCCESS) { log_error("clCreateCommandQueue", err); release(); return; }

    impl->program = clCreateProgramWithSource(impl->context, 1, &KERNEL_SOURCE, nullptr, &err);
    if(err != CL_SUCCESS) { log_error("clCreateProgramWithSource", err); release(); return; }

    err = clBuildProgram(impl->program, 1, &device, nullptr, nullptr, nullptr);
    if(err != CL_SUCCESS) {
        size_t log_size = 0;
        clGetProgramBuildInfo(impl->program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size);
        std::string log(log_size, '\0');
        clGetProgramBuildInfo(impl->program, device, CL_PROGRAM_BUILD_LOG, log_size, &log[0], nullptr);
        std::fprintf(stderr, "[GpuGravity] 커널 빌드 실패:\n%s\n", log.c_str());
        release();
        return;
    }

    struct KernelSlot { cl_kernel* kernel; const char* name; };
    const KernelSlot slots[] = {
        {&impl->refresh_kernel, "refresh_acceleration"},
        {&impl->predict_kernel, "predict"},
        {&impl->step_even, "step"},
        {&impl->step_odd, "step"},
    };

    for(const KernelSlot& slot : slots) {
        *slot.kernel = clCreateKernel(impl->program, slot.name, &err);
        if(err != CL_SUCCESS) { log_error(slot.name, err); release(); return; }
    }

    std::fprintf(stderr, "[GpuGravity] GPU 사용: %s\n", name);
    ready.store(true, std::memory_order_release);
}

void GpuGravity::release() {
    ready.store(false, std::memory_order_release);
    if(!impl) return;

    release_mem(impl->state_buf);
    release_mem(impl->pos_next_buf);
    release_mem(impl->body_a);
    release_mem(impl->body_b);
    release_mem(impl->ctrl_buf);

    release_kernel(impl->refresh_kernel);
    release_kernel(impl->predict_kernel);
    release_kernel(impl->step_even);
    release_kernel(impl->step_odd);

    if(impl->program) clReleaseProgram(impl->program);
    if(impl->queue) clReleaseCommandQueue(impl->queue);
    if(impl->context) clReleaseContext(impl->context);

    delete impl;
    impl = nullptr;
}

bool GpuGravity::ensure_capacity(std::size_t padded) {
    cl_int err = CL_SUCCESS;

    auto make = [&](cl_mem& mem, std::size_t bytes, const char* what) {
        mem = clCreateBuffer(impl->context, CL_MEM_READ_WRITE, bytes, nullptr, &err);
        if(err != CL_SUCCESS) log_error(what, err);
        return err == CL_SUCCESS;
    };

    if(!impl->ctrl_buf) {
        if(!make(impl->ctrl_buf, 4 * sizeof(cl_int), "ctrl 버퍼 생성")) return false;
    }

    if(padded <= impl->capacity) return true;

    std::size_t new_capacity = impl->capacity == 0 ? 512 : impl->capacity;
    while(new_capacity < padded) new_capacity *= 2;

    release_mem(impl->state_buf);
    release_mem(impl->pos_next_buf);
    release_mem(impl->body_a);
    release_mem(impl->body_b);

    if(!make(impl->state_buf, new_capacity * sizeof(State), "state 버퍼 생성")) return false;
    if(!make(impl->pos_next_buf, new_capacity * 2 * sizeof(double), "pos_next 버퍼 생성")) return false;
    if(!make(impl->body_a, new_capacity * sizeof(Body), "body 버퍼 생성")) return false;
    if(!make(impl->body_b, new_capacity * sizeof(Body), "body 버퍼 생성")) return false;

    impl->capacity = new_capacity;
    return true;
}

bool GpuGravity::advance(std::vector<Particle>& particles, double dt, int steps, double G, double eps_sq,
                         bool refresh_acceleration, int collision_delay,
                         int& steps_done, bool& collision) {
    steps_done = 0;
    collision = false;

    if(!available()) return false;

    const std::size_t n = particles.size();
    if(n == 0 || steps <= 0) return true;

    const std::size_t padded = (n + GROUP_SIZE - 1) / GROUP_SIZE * GROUP_SIZE;

    if(!ensure_capacity(padded)) { release(); return false; }

    impl->bodies.resize(padded);
    impl->states.resize(n);
    for(std::size_t i = 0; i < n; i++) {
        const Particle& p = particles[i];
        impl->bodies[i] = {static_cast<float>(p.get_position().x), static_cast<float>(p.get_position().y),
                           static_cast<float>(p.get_mass()), static_cast<float>(p.get_radius())};
        impl->states[i] = {p.get_position().x, p.get_position().y,
                           p.get_velocity().x, p.get_velocity().y,
                           p.get_acceleration().x, p.get_acceleration().y,
                           p.get_mass(), 0.0};
    }
    for(std::size_t i = n; i < padded; i++) {
        impl->bodies[i] = {1.0e15f, 1.0e15f, 0.0f, 0.0f};
    }

    const cl_int n_arg = static_cast<cl_int>(n);
    const cl_int padded_arg = static_cast<cl_int>(padded);
    const float g_arg = static_cast<float>(G);
    const float eps_arg = static_cast<float>(eps_sq);
    const cl_int delay_arg = collision_delay;
    const cl_int ctrl_init[4] = {-1, -1, 0, 0};

    cl_int err = CL_SUCCESS;
    cl_command_queue q = impl->queue;

    err |= clEnqueueWriteBuffer(q, impl->body_a, CL_FALSE, 0,
                                padded * sizeof(Body), impl->bodies.data(), 0, nullptr, nullptr);
    err |= clEnqueueWriteBuffer(q, impl->state_buf, CL_FALSE, 0,
                                n * sizeof(State), impl->states.data(), 0, nullptr, nullptr);
    err |= clEnqueueWriteBuffer(q, impl->ctrl_buf, CL_FALSE, 0,
                                sizeof(ctrl_init), ctrl_init, 0, nullptr, nullptr);

    const size_t local = GROUP_SIZE;
    const size_t global = padded * SLICES;
    const size_t global_bodies = padded;

    if(refresh_acceleration) {
        cl_kernel k = impl->refresh_kernel;
        err |= clSetKernelArg(k, 0, sizeof(cl_mem), &impl->state_buf);
        err |= clSetKernelArg(k, 1, sizeof(cl_mem), &impl->body_a);
        err |= clSetKernelArg(k, 2, sizeof(cl_int), &n_arg);
        err |= clSetKernelArg(k, 3, sizeof(cl_int), &padded_arg);
        err |= clSetKernelArg(k, 4, sizeof(float), &g_arg);
        err |= clSetKernelArg(k, 5, sizeof(float), &eps_arg);
        err |= clEnqueueNDRangeKernel(q, k, 1, nullptr, &global, &local, 0, nullptr, nullptr);
    }

    {
        cl_kernel k = impl->predict_kernel;
        err |= clSetKernelArg(k, 0, sizeof(cl_mem), &impl->state_buf);
        err |= clSetKernelArg(k, 1, sizeof(cl_mem), &impl->pos_next_buf);
        err |= clSetKernelArg(k, 2, sizeof(cl_mem), &impl->body_a);
        err |= clSetKernelArg(k, 3, sizeof(cl_mem), &impl->body_b);
        err |= clSetKernelArg(k, 4, sizeof(cl_int), &n_arg);
        err |= clSetKernelArg(k, 5, sizeof(double), &dt);
        err |= clEnqueueNDRangeKernel(q, k, 1, nullptr, &global_bodies, nullptr, 0, nullptr, nullptr);
    }

    auto set_step_args = [&](cl_kernel k, cl_mem in, cl_mem out, cl_int flag_in, cl_int flag_out) {
        err |= clSetKernelArg(k, 0, sizeof(cl_mem), &impl->state_buf);
        err |= clSetKernelArg(k, 1, sizeof(cl_mem), &impl->pos_next_buf);
        err |= clSetKernelArg(k, 2, sizeof(cl_mem), &in);
        err |= clSetKernelArg(k, 3, sizeof(cl_mem), &out);
        err |= clSetKernelArg(k, 4, sizeof(cl_mem), &impl->ctrl_buf);
        err |= clSetKernelArg(k, 5, sizeof(cl_int), &flag_in);
        err |= clSetKernelArg(k, 6, sizeof(cl_int), &flag_out);
        err |= clSetKernelArg(k, 7, sizeof(cl_int), &n_arg);
        err |= clSetKernelArg(k, 8, sizeof(cl_int), &padded_arg);
        err |= clSetKernelArg(k, 9, sizeof(float), &g_arg);
        err |= clSetKernelArg(k, 10, sizeof(float), &eps_arg);
        err |= clSetKernelArg(k, 11, sizeof(double), &dt);
        err |= clSetKernelArg(k, 12, sizeof(cl_int), &delay_arg);
    };
    set_step_args(impl->step_even, impl->body_b, impl->body_a, 0, 1);
    set_step_args(impl->step_odd, impl->body_a, impl->body_b, 1, 0);

    for(int s = 0; s < steps && err == CL_SUCCESS; s++) {
        cl_kernel k = (s % 2 == 0) ? impl->step_even : impl->step_odd;
        err |= clEnqueueNDRangeKernel(q, k, 1, nullptr, &global, &local, 0, nullptr, nullptr);
    }

    cl_int ctrl_out[4] = {0, 0, 0, 0};

    if(err == CL_SUCCESS) {
        err |= clEnqueueReadBuffer(q, impl->state_buf, CL_FALSE, 0,
                                   n * sizeof(State), impl->states.data(), 0, nullptr, nullptr);
        err |= clEnqueueReadBuffer(q, impl->ctrl_buf, CL_TRUE, 0,
                                   sizeof(ctrl_out), ctrl_out, 0, nullptr, nullptr);
    }

    if(err != CL_SUCCESS) {
        log_error("advance", err);
        release();
        return false;
    }

    for(std::size_t i = 0; i < n; i++) {
        const State& s = impl->states[i];
        particles[i].set_position(Vector2(s.px, s.py));
        particles[i].set_velocity(Vector2(s.vx, s.vy));
        particles[i].set_acceleration(Vector2(s.ax, s.ay));
    }

    collision = ctrl_out[0] >= 0 || ctrl_out[1] >= 0;
    steps_done = ctrl_out[2];
    return true;
}
