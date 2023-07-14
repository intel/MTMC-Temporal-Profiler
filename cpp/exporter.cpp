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

#include "exporter.h"

mtmc::ShmExporter::ShmExporter() {}

mtmc::ShmExporter::~ShmExporter() {
}

__attribute__((optimize("O3"))) int mtmc::ShmExporter::Export(std::list<util::IndexVector<SingleProfile>>& profile_storage,
                              ProfilerSetting mtmc_setting) {

    ipc::channel chnl = ipc::channel(DEFAULT_CHANNEL_NAME, ipc::sender);

    if (!chnl.valid()) {
        Dprintf("Export failed due to channel is not valid\n");
        return -1;
    }

    std::vector<SingleProfile*> to_export;
    size_t data_size_bytes = 0;

    // Calculate SHM size as well as select data to be exported
    for (auto& vec : profile_storage) {
        for (int prof_i = 0; prof_i < vec.Size(); ++prof_i) {
            auto& profile = vec[prof_i];
            /* Here are some invalid data conditions */
            if (profile.rd_ret_start.num_event != profile.rd_ret_end.num_event) {
                continue;
            }
            if (mtmc_setting.perf_collect_topdown && (profile.rd_ret_start.num_event - 2 < 0)) {
                Dprintf(FRED("Topdown metric and event number mismatch\n"));
                continue;
            }
            to_export.push_back(&profile);
            data_size_bytes += sizeof(profile);
        }
    }

    data_size_bytes += sizeof(ShmIpcStatus);
    data_size_bytes += data_size_bytes % 256; // Try to align the data trunk

    std::string shm_name = std::to_string(reinterpret_cast<uint64_t>(&profile_storage)+mtmc::Env::GetClockTimeNs());

    // Print shm settings
    Dprintf("Name: %s, Shm size: %lu, to export size: %lu\n",
           shm_name.c_str(), data_size_bytes, to_export.size());

    // Alloc SHM and export
    ipc::shm::handle shm_hdlr = ipc::shm::handle(shm_name.c_str(),
                                 data_size_bytes, ipc::shm::create);
    if (!shm_hdlr.valid()) {
        Dprintf(FRED("Export failed due to shm handler is not valid\n"));
        return -1;
    }
    memset(shm_hdlr.get(), 0, shm_hdlr.size());
    Dprintf(FCYN("Data size to export is %.2f MB. SHM size: %lu, SHM name: %s\n"),
            float(data_size_bytes)/1e6,shm_hdlr.size(), shm_hdlr.name());

    // Copy data to the SHM
    auto shm_status = (ShmIpcStatus*)(shm_hdlr.get());
    auto head = (SingleProfile*)((char*)shm_hdlr.get() + sizeof(ShmIpcStatus));
    for (int i = 0; i < to_export.size(); ++i) {
        memcpy(head + i, to_export.data()[i], sizeof(SingleProfile));
    }

    // Create Load
    auto load = ShmIpcLoad{};
    load.data_offset = 0;
    snprintf(load.shm_name, 32, "%s", shm_hdlr.name());
    snprintf(load.msg, 32, "%s", std::to_string(shm_hdlr.size()).c_str());
    load.shm_size = shm_hdlr.size();
    load.num_data = to_export.size();
    load.trace_hash = mtmc_setting.trace_hash;
    load.configs_id = mtmc_setting.configs_id;
    load.cnsts_length = mtmc_setting.cnst_var.size();
    int cntr = 0;
    for (auto& cnst : mtmc_setting.cnst_var) {
        if (cnst == "SYSTEM_TSC_FREQ") {
            load.cnsts[cntr] = mtmc::Env::GetTSCFrequencyHz();
        }
        else if (cnst == "DURATIONTIMEINMILLISECONDS") {
            load.cnsts[cntr] = -2;
        }
        else {
            printf(FRED("Error. MTMC profiler encountered an unknown constant. The post-processing may fail."
                        "Unknown Constant: %s\n"), cnst.c_str());
            load.cnsts[cntr] = -1;
        }
        ++cntr;
    }

    // Channel send requests
    bool status = chnl.send(&load, sizeof(load));
    Dprintf(FYEL("Send data %d. Waiting for response\n"), status);

    if (!status) {
        Dprintf(FRED("Send data to otle exporter failed. Make sure you have started mtmc otle exporter\n"));
        return -1;
    }

    // Wait for request done;
    for (int i = 0; i < 1000; ++i) {
        if (!shm_status->done) {
            usleep(100 * 1e3);
        }
        else {
            break;
        }
        if (i == 1000 - 1) {
            Dprintf(FRED("Timeout and does not receive done signale from mtmc otle exporter\n"));
            return -1;
        }
    }
    Dprintf(FGRN("OTLE Exporter has received the payload.\n"));
    shm_hdlr.release();

    return 0;
}

