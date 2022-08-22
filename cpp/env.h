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
#include <cstdint>
#include <vector>
#include <time.h>
#include <pthread.h>


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

// Compile settings
//#define DETAILED_PRINT
//#define DEBUG_PRINT

namespace mtmc {

    // TODO: Later all Env Class function should be platform-sensitive
    class Env {
    public:

        static uint64_t rdtsc() {
            rmb();
            uint32_t lo, hi;
            __asm volatile ("rdtsc" : "=a" (lo), "=d" (hi):: "%rbx", "%rcx");
            return ((uint64_t)hi << 32) | lo;
        }

        static void GetCoreId(uint32_t *pid, uint32_t *prefix) {
            rmb();
            __asm volatile ( "rdtscp\n" : "=c" (*pid) : : );

            // Prefix indicate the socket and pid is the actural core number
            // For example, socket 0 core 2: pid = 2, prefix = 0; socket 1 core 2: pid = 2, prefix = 0b1000000011.
            // The rule about the prefix is not clear yet.
            // [0:19] = socket id. [20:31] = core#. So we put socket id ... to prefix and core# to pid

            *prefix = (*pid) >> 12;
            *pid = ((*pid) << 20) >> 20;
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

    };

}

#endif //MTMC_ENV_H
