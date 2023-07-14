//Copyright 2022 Intel Corporation
//
//Licensed under the Apache License, Version 2.0 (the "License");
//you may not use this file except in compliance with the License.
//You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//Unless required by applicable law or agreed to in writing, software
//distributed under the License is distributed on an "AS IS" BASIS,
//WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//See the License for the specific language governing permissions and
//        limitations under the License.

#ifndef MTMC_ENV_H
#define MTMC_ENV_H

#include <sys/syscall.h>
#include <unistd.h>
#include <cmath>
#include <cstdint>
#include <vector>
#include <time.h>
#include <pthread.h>
#include <cmath>

#define GP_COUNTER 12
#define DEBUG_MODE

// X86
#ifdef __x86_64__
#include <x86intrin.h>
#endif

#define mb() asm volatile("mfence" ::: "memory")
#define rmb() asm volatile("lfence" ::: "memory")
#define wmb() asm volatile("sfence" ::: "memory")
#define cpl_barrier() asm volatile("" ::: "memory")

#define USE_PER_THREADS_PERF

// Compile settings
//#define DETAILED_PRINT
//#define DEBUG_PRINT

namespace mtmc {

    // TODO: Later all Env Class function should be platform-sensitive
    class Env {
    public:

        union CoreId {
            uint64_t full;
            struct {
                uint32_t pid;
                uint32_t prefix;
            } partial;
        };

        static uint64_t rdtsc() {
            rmb();
            uint32_t lo, hi;
            __asm volatile ("rdtsc" : "=a" (lo), "=d" (hi):: "%rbx", "%rcx");
            return ((uint64_t)hi << 32) | lo;
        }

        static inline void GetCoreId(uint32_t *pid, uint32_t *prefix) {
            rmb();
//            __asm volatile ( "rdtscp\n" : "=c" (*pid) : : );
            _rdtscp(pid);

            // Prefix indicate the socket and pid is the actural core number
            // For example, socket 0 core 2: pid = 2, prefix = 0; socket 1 core 2: pid = 2, prefix = 0b1000000011.
            // The rule about the prefix is not clear yet.
            // [0:19] = socket id. [20:31] = core#. So we put socket id ... to prefix and core# to pid

            *prefix = (*pid) >> 12;
            *pid = ((*pid) << 20) >> 20;
        }

        static __attribute__((optimize("-O2"))) inline CoreId GetCoreIdNew() {
//        static inline CoreId GetCoreIdNew() {
            rmb();
            CoreId id{};
            asm volatile ( "rdtscp\n" : "=c" (id.partial.pid) : : );
//            _rdtscp(&id.partial.pid);
            id.partial.prefix = (id.partial.pid) >> 12;
            id.partial.pid = ((id.partial.pid) << 20) >> 20;
            return id;
        }

        static int64_t GetKtid() {
            return syscall(__NR_gettid);
        }

        static int32_t GetPthreadid() {
            return (int32_t)(pthread_self());
        }

        static uint64_t GetClockTimeNs() {
            struct timespec spc{};
            clock_gettime(CLOCK_REALTIME, &spc);
            return ((uint64_t)spc.tv_sec*1000*1000*1000 + (uint64_t)spc.tv_nsec);
        }

        static uint64_t GetTSCFrequencyHz() {
            static uint64_t ret;
            if (ret == 0) {

                uint64_t tsc_start = rdtsc();
                uint64_t ns_start = GetClockTimeNs();

                usleep(5 * 1e3);

                uint64_t tsc_end = rdtsc();
                uint64_t ns_end = GetClockTimeNs();

                double temp = floor( ((double) (tsc_end - tsc_start) / (double) (ns_end - ns_start))*100 ) / 100;
                ret = (uint64_t) (temp * 1e9);
            }
            return ret;

            /// Other methods
            /// CPUID -1 | grep clock
            // cpuid -1 | grep -o -P '(?<=TSC\/clock ratio = )[0-9]+\/[0-9]+'
            // cpuid -1 | grep -o 'nominal core crystal clock = [0-9]*' | cut -d " " -f6
        };


    };

}

#endif //MTMC_ENV_H
