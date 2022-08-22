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

#ifndef MTMC_GUARD_SAMPLER_H
#define MTMC_GUARD_SAMPLER_H

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <thread>
#include <pthread.h>
#include <signal.h>
#include <fstream>

#include "env.h"
#include "util.h"

namespace mtmc {

    struct TimeInfo {
        timespec time_mo[2];
        timespec time_rl[2];
    };

    class GuardSampler {
    public:
        /**
         * @description Guard sampler should call Read before workloads begin and call Stop after workload finish.
         * It is the wrapper of Linux perf record context switch. Be careful that long running time will generated
         * a huge perf data that may occupy large space on disk.
         */
        GuardSampler();

        /**
         * @name Read
         * @param data_path: Path to store perf recorded data
         * @description: Start record context switch events
         */
        void Read(const std::string& data_path);


        /**
         * @name Stop
         * @description: Stop the perf recording
         */
        void Stop();

        /**
         * @name TimeExport
         * @param data_path Path to the target file
         * @description Export time delta between two system clock for post processing
         */
        void TimeExport(const std::string& data_path);

        ~GuardSampler();

    private:
        pid_t sampler_pid;
        TimeInfo timeinfo{};
    };

}


#endif //MTMC_GUARD_SAMPLER_H
