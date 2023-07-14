// Example CPP programs for MTMC-Temp-Profiler

#include <memory>
#include <vector>
#include <thread>
#include <random>
#include <iostream>

#include "mtmc/mtmc_temp_profiler.h"
#include "mtmc/mtmc_profiler.h"


void ProfilerExample() {

    mtmc::MTMCTemprolProfiler& prof = mtmc::MTMCTemprolProfiler::getInstance();
    prof.Init();

    // Do some work. Eg. memcpy
    // Assume L1 = 48k, L2 = 2048k, L3 = 100,000k
    const int L1_SIZE = 48 * 1e3;
    const int L2_SIZE = 2048 * 1e3;
    const int L3_SIZE = 99840 * 1e3;
    constexpr int FP_ARRAY_SIZE = ( L2_SIZE / sizeof(float) ) / 1;

    auto* buffer0 = aligned_alloc(64, FP_ARRAY_SIZE * sizeof(float));
    auto* buffer1 = aligned_alloc(64, FP_ARRAY_SIZE * sizeof(float));
    memset(buffer0, 0, FP_ARRAY_SIZE * sizeof(float));
    memset(buffer0, 0, FP_ARRAY_SIZE * sizeof(float));
    auto start_ns = mtmc::Env::GetClockTimeNs();

    /* Single Thread Test
       | -------- Region All ---------- |
       | -- Region 0 -- | -- Region 1 --| */
    prof.LogStart("Region All");

    prof.LogStart("Region 0");
    // Do some works for 300ms
    start_ns = mtmc::Env::GetClockTimeNs();
    while (mtmc::Env::GetClockTimeNs() - start_ns < 300 * 1e6) {
        memcpy(buffer1, buffer0, FP_ARRAY_SIZE * sizeof(float));
    }
    prof.LogEnd(); // Region 0

    prof.LogStart("Region 1");
    // Do some works for 300ms
    start_ns = mtmc::Env::GetClockTimeNs();
    while (mtmc::Env::GetClockTimeNs() - start_ns < 300 * 1e6) {
        memcpy(buffer1, buffer0, FP_ARRAY_SIZE * sizeof(float));
    }
    prof.LogEnd(); // Region 1

    prof.LogEnd(); // Region all
    /* Single Thread End */

    /* Multi-Thread Test */
    std::vector<std::thread> ths(10);

    auto worker = [](int i) {
        auto& prof = mtmc::MTMCTemprolProfiler::getInstance();
        prof.LogStart("Thread " + std::to_string(i)); // Thread i

        // Do some works for 300ms
        auto start_ns = mtmc::Env::GetClockTimeNs();
        auto* buffer_thread = aligned_alloc(64, FP_ARRAY_SIZE * sizeof(int));
        while (mtmc::Env::GetClockTimeNs() - start_ns < 300 * 1e6) {
            int val = rand();
            memset(buffer_thread, val, FP_ARRAY_SIZE * sizeof(int));
        }
        free(buffer_thread);

        prof.LogEnd();  // Thread i end
    };

    for (int i = 0; i < 10; ++i) {
        ths[i] = std::thread(worker, i);
    }
    for (auto& th : ths) th.join();
    /* Multi Thread End */

    /* Small amount of single span tasks */
    for (int i = 0; i < 60; ++i) {
        prof.LogStart("Region " + std::to_string(i%10));
        start_ns = mtmc::Env::GetClockTimeNs();
        while (mtmc::Env::GetClockTimeNs() - start_ns < 30 * 1e6) {
            memcpy(buffer1, buffer0, FP_ARRAY_SIZE * sizeof(float));
        }
        prof.LogEnd(); // Region 1
    }
    /* Small amount of single span tasks end */

    // ---------------------- Export and clean --------------------------

    free(buffer0);
    free(buffer1);

};

int main() {
    ProfilerExample();
    return 0;
}