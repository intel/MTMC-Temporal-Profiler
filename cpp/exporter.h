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
#ifndef MTMC_EXPORTER_H
#define MTMC_EXPORTER_H

#include "libipc/ipc.h"
#include "mtmc_profiler.h"
#include <iostream>
#include <cstring>

namespace mtmc {

#define DEFAULT_CHANNEL_NAME "mtmc_ipc_shm_channel_0"

    struct ShmIpcLoad {
        char msg[32];      // Message of this operation
        char shm_name[32]; // Name of the shared memory
        size_t shm_size;
        size_t data_offset;
        size_t num_data;
        int cnsts_length;
        uint64_t cnsts[16];
        uint64_t trace_hash;
        int configs_id;
    };

    struct ShmIpcStatus {
        int done;
        char placeholder[60];
    };

class ShmExporter : public Exporter {
public:
    ShmExporter();
    ~ShmExporter();

    int Export(std::list<util::IndexVector<SingleProfile>>& profile_storage,
               ProfilerSetting mtmc_setting) override;

    ShmExporter(const ShmExporter&) = delete;
    ShmExporter& operator=(const ShmExporter&) = delete;

    static ShmExporter& GetExporter() {
        static ShmExporter exporter;
        return exporter;
    }

private:
    std::string shm_name_;

};

}

#endif //MTMC_EXPORTER_H
