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

#ifndef MTMC_EBPF_SAMPLER_H
#define MTMC_EBPF_SAMPLER_H

#include <vector>
#include <map>
#include <BPF.h>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <utility>

#include "util.h"
#include "env.h"
#include "ebpf_common.h"

namespace mtmc {
    class EbpfSampler {
    public:
        // Non-thread safe
        static EbpfSampler& GetInstance() {
            if (ebpf_instance_ == nullptr) {
                ebpf_instance_ = new EbpfSampler();
            }
            return *ebpf_instance_;
        }

        int EbpfCtxScInit(EbpfCtxScConfig ebpf_cfg, const std::vector<int> &perf_cpus,
                          std::map<std::pair<int, int>, std::vector<std::pair<int, int>>> cpu_fd_pairs);

        int EbpfCtxScStart();

        int EbpfCtxScStop();

        int EbpfCtxScClose();

        void DebugPrintEbpfCtxData();

        void DebugExportEbpfCtxData(const char* path_cstr);

        EbpfSampler& operator=(const EbpfSampler&) = delete;
//        EbpfSampler& operator=(const EbpfSampler&&) = delete;
        EbpfSampler(const EbpfSampler&) = delete;
//        EbpfSampler(const EbpfSampler&&) = delete;

    private:
        EbpfSampler();
        ~EbpfSampler();

        enum EBPF_STATE : int {
            EXITING = -1,
            UNINITIALIZED = 0,
            WAITING = 1,
            RUNNING = 2,
        };

        int EbpfCtxStateChange(EBPF_STATE state);

        static EbpfSampler* ebpf_instance_;

        // Class private common variables
        std::map<std::pair<int, int>, std::vector<std::pair<int, int>>> cpu_fd_pairs_;
        ebpf::BPF bpf_;

        // Init variables
        std::mutex init_mux_;

        // For ebpf collecting threads
        std::mutex ebpf_mux_;
        std::condition_variable ebpf_cv_;
        std::atomic<int> running_state_{0};
        std::shared_ptr<std::thread> ebpf_collector_;

        // Data Storage
        std::shared_ptr<std::map<int, std::vector<StoreData>>> ebpf_data_map_;
    };
}

#endif //MTMC_EBPF_SAMPLER_H
