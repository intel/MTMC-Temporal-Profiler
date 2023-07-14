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

#ifndef MTMC_EBPF_COMMON_H
#define MTMC_EBPF_COMMON_H

namespace mtmc {

    struct EbpfCtxScConfig {
        uint64_t PerCoreStorageLength = 1000; // Size of per core ring buffer
        uint64_t StorageMaxByte = 1e9; // Default maximum storage for all cpus in the main memory
        uint64_t WeakUpIntvMs = 1000;
    };

    struct PmcData {
        uint64_t time_stamp;
        pid_t prev_pid;
        pid_t curr_pid;
        int core_id;
        uint64_t prev_task_state;
        uint64_t pmc_reading[13];
    };

    struct StoreData {
        uint64_t curr_idx;
        uint64_t warp_around;
        PmcData pmc_data;
    };

}

#endif //MTMC_EBPF_COMMON_H
