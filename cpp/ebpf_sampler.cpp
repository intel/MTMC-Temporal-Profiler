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

#include "ebpf_sampler.h"

namespace mtmc {

    EbpfSampler* EbpfSampler::ebpf_instance_ = nullptr;

    EbpfSampler::EbpfSampler() : ebpf_collector_(nullptr) {}

    int EbpfSampler::EbpfCtxScInit(EbpfCtxScConfig ebpf_cfg, const std::vector<int> &perf_cpus,
                                   std::map<std::pair<int, int>, std::vector<std::pair<int, int>>> cpu_fd_pairs) {

        // Make sure Init and Close only called by one thread at a time
        if (running_state_.load() > EBPF_STATE::UNINITIALIZED) {
            return -1;
        }
        std::lock_guard<std::mutex> lck(init_mux_);
        if (running_state_.load() > EBPF_STATE::UNINITIALIZED) {
            return -1;
        }

        Dprintf(FGRN("[EBPF COLLECTOR] Start Init EBPF Ctx\n"));

        cpu_fd_pairs_ = std::move(cpu_fd_pairs);
        auto& cpu_onlines = perf_cpus;

        // defines for number of event
        std::string env_df = "-DNUM_EVTS=" + std::to_string(cpu_fd_pairs_.size());

        // define length of percpu array perfmon_data
        const uint64_t EBPF_MAX_PERCORE_LENGTH = ebpf_cfg.PerCoreStorageLength;
        std::string datalen_df = "-DPERCPU_ARRAY_LENGTH=" +std::to_string(EBPF_MAX_PERCORE_LENGTH);

        // Initialize and register BPF
        const std::string BPF_PROGRAM = R"(
#include <linux/sched.h>
#include <uapi/linux/ptrace.h>

// -- New
struct pmc_data {
    u64 time_stamp;
    pid_t prev_pid;
    pid_t curr_pid;
    int core_id;
    u64 prev_task_state;
    u64 pmc_reading[13];
};

BPF_PERF_ARRAY(inst, MAX_CPUS);

BPF_PERF_ARRAY(PMC0, MAX_CPUS);
BPF_PERF_ARRAY(PMC1, MAX_CPUS);
BPF_PERF_ARRAY(PMC2, MAX_CPUS);
BPF_PERF_ARRAY(PMC3, MAX_CPUS);
BPF_PERF_ARRAY(PMC4, MAX_CPUS);
BPF_PERF_ARRAY(PMC5, MAX_CPUS);
BPF_PERF_ARRAY(PMC6, MAX_CPUS);
BPF_PERF_ARRAY(PMC7, MAX_CPUS);
BPF_PERF_ARRAY(PMC8, MAX_CPUS);
BPF_PERF_ARRAY(PMC9, MAX_CPUS);
BPF_PERF_ARRAY(PMC10, MAX_CPUS);
BPF_PERF_ARRAY(PMC11, MAX_CPUS);
BPF_PERF_ARRAY(PMC12, MAX_CPUS);
BPF_PERF_ARRAY(PMC13, MAX_CPUS);
BPF_PERF_ARRAY(PMC14, MAX_CPUS);
BPF_PERF_ARRAY(PMC15, MAX_CPUS);

BPF_PERCPU_ARRAY(counter, u64, 2); // [[CNTR_IDX],[WRAP_AROUND]]
BPF_PERCPU_ARRAY(perfmon_data, struct pmc_data, PERCPU_ARRAY_LENGTH);
// -- New

int task_switch_event(struct pt_regs *ctx, struct task_struct *prev) {
  pid_t prev_pid = prev->pid;
  pid_t cur_pid = bpf_get_current_pid_tgid();
  int cur_cpu = bpf_get_smp_processor_id();
  uint64_t cur_ts = bpf_ktime_get_ns();
  long state = prev->__state;

//    if (prev_pid == 0)
//        return 0;
//    if (state == 0x0001)
//        return 0;

    // -- New
    int CNTR_IDX = 0; // Current index in the perfmon_data. When reading, the available data should be [0, CNTR_IDX) [CNTR_IDX', PERCPU_ARRAY_LENGTH)
    int WRAP_AROUND = 1; // How many times the PERCPU_ARRAY has wrapped around
    u64* store_idx_ptr = NULL;
    store_idx_ptr = counter.lookup(&CNTR_IDX);
    if (store_idx_ptr) {
        struct pmc_data* pmc_data_ptr = NULL;
        int store_idx_int = (int)(*store_idx_ptr); // Cast int here since it needs 64bit alignment
        pmc_data_ptr = perfmon_data.lookup(&store_idx_int);
        if (pmc_data_ptr) {
            if (NUM_EVTS > 0) pmc_data_ptr->pmc_reading[0] = PMC0.perf_read(cur_cpu); // PMC read
            if (NUM_EVTS > 1) pmc_data_ptr->pmc_reading[1] = PMC1.perf_read(cur_cpu); // PMC read
            if (NUM_EVTS > 2) pmc_data_ptr->pmc_reading[2] = PMC2.perf_read(cur_cpu); // PMC read
            if (NUM_EVTS > 3) pmc_data_ptr->pmc_reading[3] = PMC3.perf_read(cur_cpu); // PMC read
            if (NUM_EVTS > 4) pmc_data_ptr->pmc_reading[4] = PMC4.perf_read(cur_cpu); // PMC read
            if (NUM_EVTS > 5) pmc_data_ptr->pmc_reading[5] = PMC5.perf_read(cur_cpu); // PMC read
            if (NUM_EVTS > 6) pmc_data_ptr->pmc_reading[6] = PMC6.perf_read(cur_cpu); // PMC read
            if (NUM_EVTS > 7) pmc_data_ptr->pmc_reading[7] = PMC7.perf_read(cur_cpu); // PMC read
            if (NUM_EVTS > 8) pmc_data_ptr->pmc_reading[8] = PMC8.perf_read(cur_cpu); // PMC read
            if (NUM_EVTS > 9) pmc_data_ptr->pmc_reading[9] = PMC9.perf_read(cur_cpu); // PMC read
            if (NUM_EVTS > 10) pmc_data_ptr->pmc_reading[10] = PMC10.perf_read(cur_cpu); // PMC read
            if (NUM_EVTS > 11) pmc_data_ptr->pmc_reading[11] = PMC11.perf_read(cur_cpu); // PMC read
            if (NUM_EVTS > 12) pmc_data_ptr->pmc_reading[12] = PMC12.perf_read(cur_cpu); // PMC read
            pmc_data_ptr->time_stamp = bpf_ktime_get_ns();
            pmc_data_ptr->curr_pid = cur_pid;
            pmc_data_ptr->prev_pid = prev_pid;
            pmc_data_ptr->core_id = cur_cpu;
            pmc_data_ptr->prev_task_state = state;
        }
        if (*store_idx_ptr == PERCPU_ARRAY_LENGTH - 1) {
            u64 val_to_update = 0;
            counter.update(&CNTR_IDX, &val_to_update);
            counter.increment(WRAP_AROUND);
        }
        else {
            counter.increment(CNTR_IDX);
        }
    }

    // -- New -- End

  return 0;
}
)";
        std::vector<std::string> cflags = {"-DMAX_CPUS="+std::to_string(cpu_onlines.size()), env_df, datalen_df};
        auto init_res = bpf_.init(BPF_PROGRAM, cflags);
        if (!init_res.ok()) {
            Dprintf(FRED("%s\n"), init_res.msg().c_str());
            return -1;
        }

        int cntr = 0;
        for (auto & single_event_cpu_fd_pair : cpu_fd_pairs_) {
            std::cout << single_event_cpu_fd_pair.second.size() << std::endl;
            auto status = bpf_.attach_curr_perffd("PMC"+std::to_string(cntr),single_event_cpu_fd_pair.second);
            if (!status.ok()) {
                Dprintf("%s\n", status.msg().c_str());
                return -1;
            }
            cntr += 1;
        }
        std::cout << cntr << std::endl;

        auto attach_res = bpf_.attach_kprobe("finish_task_switch", "task_switch_event");
        if (!attach_res.ok()) {
            Dprintf(FRED("%s\n"), attach_res.msg().c_str());
            return -1;
        }

        // Calculate perfmon_collector eBPF max data storage length
        int max_cpu = util::GetMaxNumOfCpus();
        int num_evt = cpu_fd_pairs_.size();
        const uint64_t kper_data_pmc_bytes = num_evt * sizeof(uint64_t);
        const uint64_t kfull_data_bytes = sizeof(StoreData);
        const uint64_t kper_core_length = ebpf_cfg.StorageMaxByte/((kfull_data_bytes)*cpu_onlines.size());

        Dprintf(FCYN("Kper_core_length: %lu, kper_data_pmc_bytes: %lu, kmeta_data_bytes: %lu\n"), kper_core_length,
                kper_data_pmc_bytes);

        // Init perfmon_collector eBPF data storage. With maximum
        ebpf_data_map_ = std::make_shared<std::map<int, std::vector<StoreData>>>();
        for (auto& olcpu : cpu_onlines) {
            ebpf_data_map_->insert({olcpu, std::vector<StoreData>()});
            ebpf_data_map_->find(olcpu)->second.reserve(kper_core_length);
        }

        // Thread for constantly acquiring data
        running_state_.store(EBPF_STATE::WAITING);
        auto fn = [=]{
            std::cout << FYEL("FLAG3") << std::endl;
            auto percpu_perfmon_cntr = this->bpf_.get_percpu_array_table<uint64_t>("counter");
            auto percpu_perfmon_data = this->bpf_.get_percpu_array_table<PmcData>("perfmon_data");
            std::vector<PmcData> keys(max_cpu * ebpf_cfg.PerCoreStorageLength);
            std::vector<PmcData> values(max_cpu * ebpf_cfg.PerCoreStorageLength);
            // Outer loop -- Check between each start / Stop
            while(this->running_state_.load() != EBPF_STATE::EXITING) {
                // Wait for start
                {
                    ebpf_cv_.notify_all();
                    std::unique_lock<std::mutex> lck(this->ebpf_mux_);
                    while (this->running_state_.load() != EBPF_STATE::RUNNING) {
                        // Exiting conditions
                        if (this->running_state_.load() == EBPF_STATE::EXITING) goto EXIT;
                        ebpf_cv_.wait(lck);
                        if (this->running_state_.load() == EBPF_STATE::EXITING) goto EXIT;
                    }
                }

                // Per iteration time
                auto startts = Env::GetClockTimeNs();

                // Check storage size
                uint64_t storage_usage = 0;
                for (auto& store_data : *ebpf_data_map_) {
                    storage_usage += store_data.second.size() * kfull_data_bytes;
                }
                if (storage_usage > ebpf_cfg.StorageMaxByte) {
                    Dprintf(FRED("[EBPF COLLECTOR] Collector storage exceeds limits. Current %lu. Max %lu."),
                            storage_usage, ebpf_cfg.StorageMaxByte);
                    std::lock_guard<std::mutex> lck(ebpf_mux_);
                    running_state_.store(EBPF_STATE::EXITING);
                    continue;
                }

                // Loop through per cpu data
                std::vector<std::vector<uint64_t>> per_cpu_pmc_cntr = percpu_perfmon_cntr.get_table_offline();
//                std::vector<std::vector<PmcData>> per_cpu_pmc_data = percpu_perfmon_data.get_table_offline();
                int fd_pmc = percpu_perfmon_data.get_fd();
                __u32 out_batch = 0;
                __u32 count = 0;
                uint64_t total = 0;
                while(true) {
                    count = ebpf_cfg.PerCoreStorageLength - total;
                    int ret = bpf_lookup_batch(fd_pmc, total == 0 ? NULL:&out_batch, &out_batch, keys.data(),
                                               values.data(), &count);
                    total += count;
                    Dprintf(FMAG("Ret %d, count %d, total %lu\n"), ret, count, total);
                    if (ret != 0)
                        break;
                    if (total == ebpf_cfg.PerCoreStorageLength)
                        break;
                    if (count == 0)
                        break;
                }

                for (auto& cpu_id : cpu_onlines) {
                    // Get per core storage
                    auto itr = this->ebpf_data_map_->find(cpu_id);
                    if (itr == this->ebpf_data_map_->end())
                        continue;
                    auto& storage = itr->second;

                    // Get per cpu ebpf data header
                    uint64_t& curr_id = per_cpu_pmc_cntr[0][cpu_id];
                    uint64_t& warp_around = per_cpu_pmc_cntr[1][cpu_id];

                    // Check the difference between this sample and the last one.
                    bool valid = true;
                    bool warp_error = false;
                    uint64_t last_id = 0;
                    uint64_t last_warp_around = 0;
                    if (!storage.empty()) {
                        last_id = storage.back().curr_idx;
                        last_warp_around = storage.back().warp_around;
                    }
                    if (warp_around - last_warp_around > 1) { // Invalid if warp_around more than once
                        warp_error = true;
                        Dprintf(FRED("[EBPF COLLECTOR] Core %d data invalid due to storage overlap. Last warp around: %lu,"
                                     "current warp around: %lu\n"), cpu_id, last_warp_around, warp_around);
                    }
                    if (storage.size() > kper_core_length) { // Invalid if storage is full
                        valid = false;
                        Dprintf(FRED("[EBPF COLLECTOR] Core %d data invalid due to storage is full\n"), cpu_id);
                    }

                    // Collect data
                    if (valid) {
                        // If warp around more than once, the data is regarded as invalid, and only store |xxxxxxC________|
                        if (warp_error) {
                            // Begin -> Curr
                            for (uint64_t i = 0; i < curr_id; ++i) {
                                storage.push_back(StoreData{.curr_idx = curr_id, .warp_around = warp_around,
//                                        .pmc_data = per_cpu_pmc_data[i][cpu_id]});
                                        .pmc_data = values[i*max_cpu+cpu_id]});
                            }
                        }
                            // Store the data
                        else {
                            // |xxxxxC_____Lxxxxx|
                            if (last_id > curr_id) {
                                // Last -> End
                                for (uint64_t i = last_id; i < EBPF_MAX_PERCORE_LENGTH; ++i) {
                                    storage.push_back(StoreData{.curr_idx = curr_id, .warp_around = warp_around,
//                                            .pmc_data = per_cpu_pmc_data[i][cpu_id]});
                                            .pmc_data = values[i*max_cpu+cpu_id]});
                                }

                                // Begin -> Curr
                                for (uint64_t i = 0; i < curr_id; ++i) {
                                    storage.push_back(StoreData{.curr_idx = curr_id, .warp_around = warp_around,
//                                            .pmc_data = per_cpu_pmc_data[i][cpu_id]});
                                            .pmc_data = values[i*max_cpu+cpu_id]});
                                }
                            }
                            // |____LxxxC_______|
                            if (curr_id > last_id) {
                                // Last -> Curr
                                for (uint64_t i = last_id; i < curr_id; ++i) {
                                    storage.push_back(StoreData{.curr_idx = curr_id, .warp_around = warp_around,
//                                            .pmc_data = per_cpu_pmc_data[i][cpu_id]});
                                            .pmc_data = values[i*max_cpu+cpu_id]});
                                }
                            }
                        }
                    }
                }

                // Per iteration time
                auto endts = Env::GetClockTimeNs();
                Dprintf(FMAG("[EBPF COLLECTOR] Time per ebpf collector iteration: %lu\n"), endts - startts);
                usleep(ebpf_cfg.WeakUpIntvMs * 1000);
            }
            EXIT:
            Dprintf(FYEL("[EBPF COLLECTOR] collector Exit flag\n"));
            return;
        };

        ebpf_collector_.reset();
        ebpf_collector_ = std::make_shared<std::thread>(fn);

        return 1;
    }

    int EbpfSampler::EbpfCtxStateChange(EbpfSampler::EBPF_STATE state) {

        if (!ebpf_collector_) return -1;
        if (!ebpf_collector_->joinable()) return -1;

        {
            std::lock_guard<std::mutex> lck(ebpf_mux_);
            // If the thread is exiting, it should not change perform any other actions, so prevent the state from changing to others
            if (running_state_.load() == EBPF_STATE::EXITING)
                return -1;
            running_state_.store(state);
        }

        ebpf_cv_.notify_all();
        return 1;

        return 0;
    }

    int EbpfSampler::EbpfCtxScStart() {
        Dprintf(FYEL("Flag %s\n"), __func__);
        return EbpfCtxStateChange(EBPF_STATE::RUNNING);
    }

    int EbpfSampler::EbpfCtxScStop() {
        Dprintf(FYEL("Flag %s\n"), __func__);
        return EbpfCtxStateChange(EBPF_STATE::WAITING);
    }

    int EbpfSampler::EbpfCtxScClose() {
        // Make sure Init and Close only called by one thread at a time
        if (running_state_.load() <= EBPF_STATE::UNINITIALIZED) {
            return -1;
        }
        std::lock_guard<std::mutex> lck(init_mux_);
        if (running_state_.load() <= EBPF_STATE::UNINITIALIZED) {
            return -1;
        }

        Dprintf(FGRN("[EBPF COLLECTOR] Close eBPF context\n"));

        int success = EbpfCtxStateChange(EBPF_STATE::EXITING);

        if (success == -1) {
            Dprintf(FRED("[EBPF COLLECTOR] eBPF state change to EXITING failed\n"));
        }

        if (ebpf_collector_) {

            ebpf_collector_->join();
            ebpf_collector_.reset();
        }

        // Detach perffd
        int cntr = 0;
        for (auto& single_event_cpu_fd_pair : cpu_fd_pairs_) {
            auto status = bpf_.detach_curr_perffd("PMC"+std::to_string(cntr),single_event_cpu_fd_pair.second);
            if (!status.ok()) {
                Dprintf(FRED("[EBPF COLLECTOR] Error closing Ebpf. Msg: %s\n"), status.msg().c_str());
                return -1;
            }
            cntr++;
        }

        // Detach kprobe
        auto detach_res = bpf_.detach_kprobe("finish_task_switch");
        if (!detach_res.ok()) {
            Dprintf(FRED("[EBPF COLLECTOR] Detach kprobe error: %d. May not enable eBPF function \n"), detach_res.msg().c_str());
            return -1;
        }

        return 1;
    }

    void EbpfSampler::DebugPrintEbpfCtxData() {
        if (!ebpf_data_map_)
            return;
        for (auto& cpu_data : *ebpf_data_map_) {
            printf("CPU [%d]: Data [%lu]. ", cpu_data.first, cpu_data.second.size());
            for (int i = 0; i < (cpu_data.second.size() > 5 ? 5 : cpu_data.second.size()); ++i) {
                auto& data = cpu_data.second[i];
                printf("[(%lu, %lu)-(%d-0x%.4lX->%d)],", data.warp_around, data.curr_idx,
                       data.pmc_data.prev_pid, data.pmc_data.prev_task_state, data.pmc_data.curr_pid);
            }
            printf("\n");
        }
    }

    void EbpfSampler::DebugExportEbpfCtxData(const char* path_cstr) {
        auto path = std::string(path_cstr);
        if (!ebpf_data_map_)
            return;

        std::ofstream resultfs;
        resultfs.open(path, std::ios::out);
        if (!resultfs.good()) {
            Dprintf("Open file failed: %s\n", path.c_str());
            return;
        }

        timespec mono;
        timespec real;
        clock_gettime(CLOCK_MONOTONIC, &mono);
        clock_gettime(CLOCK_REALTIME, &real);

        auto mono_ns = (int64_t)mono.tv_sec * 1000 * 1000 * 1000 + (int64_t)mono.tv_nsec;
        auto real_ns = (int64_t)real.tv_sec * 1000 * 1000 * 1000 + (int64_t)real.tv_nsec;
        auto diff = real_ns - mono_ns;

        resultfs << mono_ns << ",";
        resultfs << real_ns << "\n";

        for (auto& cpu_data : *ebpf_data_map_) {
            for (int i = 0; i < cpu_data.second.size(); ++i) {
                auto& data = cpu_data.second[i];
                resultfs << cpu_data.first << ","; // cpu
                resultfs << data.pmc_data.time_stamp + diff << "," << data.warp_around << "," << data.curr_idx << ","; // time mono ns, warp around, idx
                resultfs << data.pmc_data.prev_task_state << "," << data.pmc_data.prev_pid << "," << data.pmc_data.curr_pid;

                // PMC readings
                resultfs << "," << data.pmc_data.pmc_reading[0];
                for (int j = 1; j < 13; ++j) {
                    resultfs << "-" << data.pmc_data.pmc_reading[j];
                }
                resultfs << "\n";
            }
        }
    }

    EbpfSampler::~EbpfSampler() {
        EbpfCtxScClose();
    }

}