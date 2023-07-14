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

#include "guard_sampler.h"

namespace mtmc {

    GuardSampler::GuardSampler() {
        sampler_pid = -1;
    }

    void GuardSampler::Read(const std::string& data_path) {
        if (sampler_pid != -1) {
            Dprintf(FRED("Sampler already running!\n"));
            return;
        }

        pid_t pid = fork();

        if (pid < 0) {
            Dprintf(FRED("Error forking a new process for perf reading\n"));
            return;
        }

        if (pid == 0) {
            Dprintf(FGRN("Child process\n"));
            const char* const argv[]  = {"perf", "record", "-e", "sched:sched_switch", "-k" ,"1", "-T", "-o" ,data_path.c_str(), NULL};
            const char* const arge[] = {NULL};

            execve("/bin/perf", const_cast<char*const*>(argv), const_cast<char*const*>(arge));
        }
        else {
            clock_gettime(CLOCK_MONOTONIC, &timeinfo.time_mo[0]);
            clock_gettime(CLOCK_REALTIME, &timeinfo.time_rl[0]);
            sampler_pid = pid;
            Dprintf(FGRN("Subprocess {%d} start collecting context switch information\n"), (int)pid);
        }
    }

    void GuardSampler::TimeExport(const std::string &data_path) {
        std::ofstream out(data_path.c_str(), std::ios::out);
        if (!out.good()) {
            Dprintf(FRED("Invalid path for time exporting\n"));
            return;
        }
        out << (uint64_t)timeinfo.time_rl[0].tv_sec * 1000 * 1000 * 1000 + (uint64_t)timeinfo.time_rl[0].tv_nsec << ",";
        out << (uint64_t)timeinfo.time_mo[0].tv_sec * 1000 * 1000 * 1000 + (uint64_t)timeinfo.time_mo[0].tv_nsec << "\n";
        out << (uint64_t)timeinfo.time_rl[1].tv_sec * 1000 * 1000 * 1000 + (uint64_t)timeinfo.time_rl[1].tv_nsec << ",";
        out << (uint64_t)timeinfo.time_mo[1].tv_sec * 1000 * 1000 * 1000 + (uint64_t)timeinfo.time_mo[1].tv_nsec;
        out.close();
    }

    GuardSampler::~GuardSampler() {
        Stop();
    }

    void GuardSampler::Stop() {
        if (sampler_pid > 0 ) {
            clock_gettime(CLOCK_MONOTONIC, &timeinfo.time_mo[1]);
            clock_gettime(CLOCK_REALTIME, &timeinfo.time_rl[1]);
            kill(sampler_pid, SIGINT);
        }
    }

}