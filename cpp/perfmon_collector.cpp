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

#include "perfmon_collector.h"

namespace mtmc {

    uint64_t ReadMmapPMC(void *addr, const ReadSetting& rd_setting) {
        auto perf_page = static_cast<perf_event_mmap_page *>(addr);

        uint32_t seq, index, width;
        uint64_t count;
        int64_t pmc;

        do {
            seq=perf_page->lock;
            cpl_barrier();

            index = perf_page->index;

            width = perf_page->pmc_width;
            if (rd_setting.add_offset) {
                count = perf_page->offset;
            }
            else {
                count = 0;
            }
            if (rd_setting.sign_ext) {
                count<<=(64-width);
                count>>=(64-width);
            }

            if (perf_page->cap_user_rdpmc && index) {

                pmc = _rdpmc(index-1);

                if (rd_setting.sign_ext) {
                    pmc<<=(64-width);
                    pmc>>=(64-width);
                }

                count+=pmc;
            }
            else {
                DDprintf(FRED("User space rdpmc is disabled or pmc index is invalid, index %d, cap_rdpmc %d\n"), index, perf_page->cap_user_rdpmc);
                return 0;
            }
            cpl_barrier();

        } while (perf_page->lock != seq);

        return count;
    }

    // ------------------------------- PerfmonAgent -------------------------------------

    int PerfmonAgent::AddAttr(const InputConfig &configs) {
//        if (configs.event_num + num_events_here_ > GP_COUNTER) {
//            Dprintf("Number of events exceeds limitation when adding\n");
//            return 0;
//        }
        cfg_vec_.push_back(configs);
        num_events_here_ += configs.event_num;
        return 1;
    }

    int PerfmonAgent::RegisterEvents(std::vector<InputConfig> sub_cfg_vec) {
        // TODO: Register failed should have corresponding clear up actions
        /* Check and iterate through cfg_vec. Each iteration register one group of events */
        for (int i = 0; i < sub_cfg_vec.size(); ++i) {
            DDprintf("Register group #%d, Events: %d\n", i, sub_cfg_vec[i].event_num);
            InputConfig& this_cfg = sub_cfg_vec[i];
            EventCtx this_event{};
            this_event.fd[0] = -1; // Set leader event fd to -1

            // We will use group[0]'s multiplex intv as the whole group's intv
            if (i == 0 && this_cfg.multiplex_intv > 0) {
                multiplex_intv = this_cfg.multiplex_intv;
                multiplex_deadline = Env::GetClockTimeNs() + multiplex_intv;
            }

            /* Register this group. */
            for (int j = 0; j < this_cfg.event_num; ++j) {

                /* Perf event open to get event fd */
                this_event.fd[j] = perf_event_open(&(this_cfg.attr_arr[j]), this_cfg.pid_arr[j], this_cfg.cpu_arr[j], this_event.fd[0],0);
                if (this_event.fd[j] < 0) {
                    Dprintf("Failed perf event open (i,j,fd,errno):%d, %d, %d, %d\n",i,j,this_event.fd[j],errno);
                    std::cout << std::hex << this_cfg.attr_arr[j].config << std::endl;
                    std::cout << this_cfg.pid_arr[j] << std::endl;
                    std::cout << std::dec << this_cfg.cpu_arr[j] << std::endl;
                    goto Failed;
                }

                /* ioctl perf event fd to a specific id */
                int ret = ioctl(this_event.fd[j], PERF_EVENT_IOC_ID, &this_event.id[j]);
                if (ret == -1) {
                    Dprintf("Failed ioctl (i,j,fd,errno):%d, %d, %d, %d\n",i,j,this_event.fd[j],ret);
                    close(this_event.fd[j]);
                    goto Failed;
                }

                /* mmap event fd. Get the address of the userspace ring buffer */
                this_event.addr[j] = mmap(NULL, (1+1)*getpagesize(), PROT_READ, MAP_SHARED, this_event.fd[j], 0);
                if (!this_event.addr[j] || this_event.addr[j] == (void*)-1) {
                    Dprintf("Failed mmap (i,j,fd,errno):%d, %d, %d, %d\n",i,j,this_event.fd[j],errno);
                    close(this_event.fd[j]);
                    goto Failed;
                }

//                std::cout << this_event.id[j] << ", "<< this_event.fd[j] << "," <<this_event.addr[j] << std::endl;

                auto perf_mmap = static_cast<perf_event_mmap_page*>(this_event.addr[j]);
                DDprintf("Register event: %llu, %d, %d, %d, rdpmcid: %d\n",
                         this_cfg.attr_arr[j].config, this_cfg.cpu_arr[j], this_cfg.pid_arr[j], this_event.fd[0], perf_mmap->index);
                this_event.event_num += 1;
            }
            this_event.rd_setting = this_cfg.rd_setting;
            this_event.last_reset_tsc = 0;
            ctx_vec_.push_back(this_event);
        }
        return 1;

        Failed:
        UnregisterEvents();
        return -1;
    };

    int PerfmonAgent::RegisterEvents() {
        return RegisterEvents(cfg_vec_);
    }

    int PerfmonAgent::UnregisterEvents() {
        for (auto & i : ctx_vec_) {
            for (int j = 0; j < i.event_num; ++j) {
                auto ret = munmap(i.addr[j], (1+1)*getpagesize());
                close(i.fd[j]);
//                printf(FYEL("Mnumap perfmon agent on tid %ld, %d\n"), Env::GetKtid(), ret);
            }
        }
        ctx_vec_.clear();
        multiplex_intv = 0;
        multiplex_deadline = 0;
        return 1;
    }

    int PerfmonAgent::EnableEvents() {
        if (num_events_here_ <= 0 || cfg_vec_.empty() || ctx_vec_.empty()) {
            Dprintf("Warring enabling events: number of registered events less than 1.\n");
            return -1;
        }
        for (int i = 0; i < ctx_vec_.size(); ++i) {
            if (IoctlSingleEventCtx(i, PERF_EVENT_IOC_ENABLE) != 1) {
                Dprintf("Enable event failed at %d ctx\n", i);
                return -1;
            }
        }
        return 1;
    }

    int PerfmonAgent::ResetEvents() {
        if (num_events_here_ <= 0 || cfg_vec_.empty() || ctx_vec_.empty()) {
            DDprintf("Warring resetting events: number of registered events less than 1.\n");
            return -1;
        }
        for (int i = 0; i < ctx_vec_.size(); ++i) {
            if (IoctlSingleEventCtx(i, PERF_EVENT_IOC_RESET) != 1) {
                Dprintf("Reset event failed at %d ctx\n", i);
                return -1;
            }
            ctx_vec_[i].last_reset_tsc = Env::rdtsc();
        }
        return 1;
    }

    int PerfmonAgent::ResetEventsAllFd() {
        if (num_events_here_ <= 0 || cfg_vec_.empty() || ctx_vec_.empty()) {
            DDprintf("Warring resetting events: number of registered events less than 1.\n");
            return -1;
        }
        for (int i = 0; i < ctx_vec_.size(); ++i) {
            for (int ev = 0; ev < ctx_vec_[i].event_num; ++ev) {
                ioctl(ctx_vec_[i].fd[ev], PERF_EVENT_IOC_RESET, 0);
            }
            ctx_vec_[i].last_reset_tsc = Env::rdtsc();
        }
        return 1;
    }

    int PerfmonAgent::EnableEventsAllFd() {
        if (num_events_here_ <= 0 || cfg_vec_.empty() || ctx_vec_.empty()) {
            Dprintf("Warring enabling events: number of registered events less than 1.\n");
            return -1;
        }
        for (int i = 0; i < ctx_vec_.size(); ++i) {
            for (int ev = 0; ev < ctx_vec_[i].event_num; ++ev) {
                ioctl(ctx_vec_[i].fd[ev], PERF_EVENT_IOC_ENABLE, 0);
            }
        }
        return 1;
    }

    int PerfmonAgent::DisableEventsAllFd() {
        if (num_events_here_ <= 0 || cfg_vec_.empty() || ctx_vec_.empty()) {
            DDprintf("Warring disabling events: number of registered events less than 1.\n");
            return -1;
        }
        for (int i = 0; i < ctx_vec_.size(); ++i) {
            for (int ev = 0; ev < ctx_vec_[i].event_num; ++ev) {
                ioctl(ctx_vec_[i].fd[ev], PERF_EVENT_IOC_DISABLE, 0);
            }
        }
        return 1;
    }

    int PerfmonAgent::DisableEvents() {
        if (num_events_here_ <= 0 || cfg_vec_.empty() || ctx_vec_.empty()) {
            DDprintf("Warring disabling events: number of registered events less than 1.\n");
            return -1;
        }
        for (int i = 0; i < ctx_vec_.size(); ++i) IoctlSingleEventCtx(i, PERF_EVENT_IOC_DISABLE);
        return 1;
    }

    int PerfmonAgent::IoctlSingleEventCtx(int ctxNum, unsigned long request) {
        if (ctx_vec_.size() <= ctxNum || cfg_vec_.size() <= ctxNum) {
            Dprintf("Warring ioctl single events ctx: event context index out of range\n");
            return -1;
        }
        for (int ev = 0; ev < ctx_vec_[ctxNum].event_num; ++ev) {
            int ret = ioctl(ctx_vec_[ctxNum].fd[ev], request, 0);
            if (ret == -1) {
                Dprintf("Error ioctl single event ctx: ioctl return error code -1 for request: %ld. Context: %d\n", request, ctxNum);
                return -1;
            }
        }
        return 1;
    }

    EventCtx *PerfmonAgent::GetEventContext(int ctxNum) {
        if (ctxNum >= ctx_vec_.size()) {
            Dprintf("EventContext index exceed total number of ctx\n");
            return nullptr;
        }
        return &(ctx_vec_[ctxNum]);
    }

    InputConfig *PerfmonAgent::GetTesterConfig(int ctxNum) {
        if (ctxNum >= cfg_vec_.size()) {
            Dprintf("EventContext index exceed total number of Tester Config\n");
            return nullptr;
        }
        return &(cfg_vec_[ctxNum]);
    }

    int PerfmonAgent::GetEventCtxNum() {
        return ctx_vec_.size();
    }

    int PerfmonAgent::ReadCounter(int print) {
        int pmuCntr = 0;
        bool success = true;
        memset(pmc_result_, 0, GP_COUNTER * sizeof(uint64_t));
        for (int envNum = 0; envNum < this->GetEventCtxNum(); ++envNum) {
            if (print) {
                printf(FYEL("rdpmc end point for event context %d\n"), envNum);
            }
            for (int i = 0; i < ctx_vec_[envNum].event_num; ++i) {
                auto perfPage = static_cast<perf_event_mmap_page *>(ctx_vec_[envNum].addr[i]);
                if (print) {
                    printf(FBLU("Events No. %d, fd: %d, id: %d, rdpmc_id: %d\n"), i, ctx_vec_[envNum].fd[i],
                           ctx_vec_[envNum].id[i], perfPage->index);
                }
                if (perfPage->index == 0) {
                    success = false;
                }
                pmc_result_[pmuCntr] = ReadMmapPMC(perfPage, ctx_vec_[envNum].rd_setting);
                if (print) {
                    printf("mmap_read_self value: %lu.\n", pmc_result_[pmuCntr]);
                }
                pmuCntr += 1;
            }
        }
        return success ? pmuCntr : -1;
    }

    int PerfmonAgent::perf_event_open(struct perf_event_attr *attr, pid_t pid, int cpu, int group_fd, unsigned long flags)  {
        return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
    }

    int PerfmonAgent::CheckAndResetEventCtx() {
        for (auto& event_ctx : ctx_vec_) {
            if (event_ctx.rd_setting.min_reset_intrvl_ns >= 0 && (Env::rdtsc() - event_ctx.last_reset_tsc) > event_ctx.rd_setting.min_reset_intrvl_ns) {
                int ret = ioctl(event_ctx.fd[0], PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
                event_ctx.last_reset_tsc = Env::rdtsc();
                if (ret == -1) {
                    Dprintf(FRED("Check and reset ctx failed with ioctl returning -1\n"));
                    return -1;
                }
            }
        }
        return 1;
    }

#ifdef USE_PER_THREADS_PERF

    int PerfmonAgent::TryInitPerThreadAgent(std::vector<InputConfig>* cfg_from_collector) {

        if (cfg_from_collector == nullptr) {
            goto Failed;
        }

        ResetEventsAllFd();
        DisableEventsAllFd();
        UnregisterEvents();
        cfg_vec_.clear();
        ctx_vec_.clear();
        num_events_here_ = 0;

        for (auto& config : *cfg_from_collector) {
            for (int j = 0; j < config.event_num; ++j) {
                config.pid_arr[j] = 0;
                config.cpu_arr[j] = -1;
            }
            this->AddAttr(config);
        }

        if (this->RegisterEvents({(*cfg_from_collector)[0]}) != 1) {
            goto Failed;
        }
        if (this->ResetEvents() != 1) {
            goto Failed;
        }
        if (this->EnableEvents() != 1) {
            goto Failed;
        }


Success:
        this->init_status = 1;
        return 1;

Failed:
        this->init_status = -1;
        return -1;
    }

    PerfmonAgent::~PerfmonAgent() {
        this->DisableEventsAllFd();
        this->UnregisterEvents();
    }

#endif

    int PerfmonAgent::MultiplexStep() {
        // multiplex_intev or deadline is 0. This means the agent do not need to switch events
        if (multiplex_intv == 0 || multiplex_deadline == 0) {
            return 0;
        }

        auto curr_time = Env::GetClockTimeNs();

        if (curr_time > multiplex_deadline) {
#if 0
            auto t0 = Env::GetClockTimeNs();
#endif
            curr_cfg_idx = util::GetNextIdxOfVec(cfg_vec_, curr_cfg_idx);
            UnregisterEvents();
            RegisterEvents({cfg_vec_[curr_cfg_idx]});
#if 0
            auto t1 = Env::GetClockTimeNs();
            Dprintf("Thread %d switch to cfg group %d. Dur: %.2f us\n", Env::GetKtid(), curr_cfg_idx, float(t1-t0)/1e3);
#endif
            return 1;
        }

        return 0;
    }

    inline int PerfmonAgent::GetMultiplexIdx() const {
        return curr_cfg_idx;
    }

    // ------------------------------- Perfmon Collector -------------------------------

    PerfmonCollector::PerfmonCollector() {
        Dprintf("PerfmonCollector ptr: %p", this);
        ready_.store(false);
    }

    void PerfmonCollector::DebugPrint() {
        Dprintf("PerfmonCollector Dprintf: %d\n", 0);

        auto available_cores = util::GetCurrAvailableCPUList();
        Dprintf("Available cores: [");
        for (auto& core : available_cores) {
            printf("%d,", core);
        }
        printf("]\n");
    }

    int PerfmonCollector::InitContext(std::vector<InputConfig> &input_config, const std::vector<int>& num_core,
                                      const ProfilerSetting& mtmc_setting) {
        if (ready_.load()) {
            Dprintf(FRED("Init failed. PerfmonCollector has already inited\n"));
            return -1;
        }

#ifdef USE_PER_THREADS_PERF

        Dprintf(FCYN("Perfmon collector will bind to THREADS\n"));

        this->init_config = input_config;

        for (auto& config : init_config) {
            for (int j = 0; j < config.event_num; ++j) {
                config.pid_arr[j] = 0;
                config.cpu_arr[j] = -1;
            }
        }

        init_cntr.fetch_add(1);

#else
        int max_num_core = util::GetMaxNumOfCpus();

        if (max_num_core > perfmon_agent_vec_.size()) {
            perfmon_agent_vec_.resize(max_num_core);
            Dprintf(FBLU("Reset perfmon_agent_vec_ to %d\n"), max_num_core);
        }

        /* Create and register per core perfmon agent */
        for (auto& cpu_id : num_core) {
            if (cpu_id >= max_num_core || cpu_id < 0) {
                Dprintf(FRED("Invalid core number %d. Ignore this config\n"), cpu_id);
                continue;
            }
            for (auto& config : input_config) {
                for (auto& cpu : config.cpu_arr) cpu = cpu_id;
                perfmon_agent_vec_[cpu_id].AddAttr(config);
            }
            if (perfmon_agent_vec_[cpu_id].RegisterEvents() != 1) return -1;
            if (perfmon_agent_vec_[cpu_id].ResetEvents() != 1) return -1;
            if (perfmon_agent_vec_[cpu_id].EnableEvents() != 1) return -1;
        }

#endif

        // TODO: Debug print, delete this part later
#ifndef USE_PER_THREADS_PERF
#ifdef DEBUG_PRINT
        Dprintf(FCYN("Init on following cores: ["));
        for (auto& core : num_core) {
            Nprintf("%d,", core);
        }
        Nprintf("]\n");
#endif
#endif

        ready_.store(true);
        return 1;
    }

    std::vector<PerfmonAgent> PerfmonCollector::DebugAcquirePerfmonAgent() {
        return perfmon_agent_vec_;
    };

#ifdef USE_PER_THREADS_PERF

    PerfmonAgent& PerfmonCollector::GetPerThreadAgent() {
        thread_local PerfmonAgent per_thread_agent;
        thread_local int this_init_cnt = 0;

        if (per_thread_agent.init_status == 0 || init_cntr.load(std::memory_order_relaxed) > this_init_cnt) {
            Dprintf(FCYN("Init perfmon agent on tid %ld\n"), Env::GetKtid());
            per_thread_agent.TryInitPerThreadAgent(&this->init_config);
            this_init_cnt = init_cntr.load(std::memory_order_relaxed);
        }

        return per_thread_agent;
    }

#endif

    int PerfmonCollector::PerCoreRead(bool reset_flag, uint64_t* ret, ReadResult* rd_ret, int* multiplex_group_idx) {
        // Get core and socket id
        Env::GetCoreId(&(rd_ret->core_id), &(rd_ret->prefix));
//        Env::CoreId id = Env::GetCoreIdNew();

//        rd_ret->core_id = id.partial.pid;
//        rd_ret->prefix = id.partial.prefix;
#ifdef USE_PER_THREADS_PERF
        if (!ready_.load()) {
            rd_ret->num_event = 0;
            return -1;
        }
        PerfmonAgent& perfmon_agent_ref = GetPerThreadAgent();

#else
        // Return if perfmon_agent_vec does not exist or have no events
        if (!ready_.load() || rd_ret->core_id >= perfmon_agent_vec_.size() || perfmon_agent_vec_[rd_ret->core_id].GetEventCtxNum() == 0) {
            rd_ret->num_event = 0;
            return -1;
        }

        // Get per core perfmon agent
        PerfmonAgent& perfmon_agent_ref = perfmon_agent_vec_[rd_ret->core_id];

#endif

        auto num_event_ctx = perfmon_agent_ref.GetEventCtxNum();

        // For start, check if pmc need reset, and if it needs to multiplex
        if (reset_flag) {
            perfmon_agent_ref.MultiplexStep();
            perfmon_agent_ref.CheckAndResetEventCtx();
            cpl_barrier();
        }

        if (multiplex_group_idx) {
            (*multiplex_group_idx) = perfmon_agent_ref.GetMultiplexIdx();
        }

        // Read those pmc and store to the ret ptr
        int ret_idx = 0;
        for (int i = 0; i < num_event_ctx; ++i) {
            EventCtx* event_ctx = perfmon_agent_ref.GetEventContext(i);
            if (!event_ctx) {
                Dprintf(FRED("Perf core read failed due to event context is NULL\n"));
                return -1;
            }
            for (int j = 0; j < event_ctx->event_num; ++j) {
                ret[ret_idx] = ReadMmapPMC(event_ctx->addr[j], event_ctx->rd_setting);
                ret_idx += 1;
            }
        }

        rd_ret->num_event = ret_idx;
        return 1;
    }

    int PerfmonCollector::CloseContext() {
//#ifndef USE_PER_THREADS_PERF
        bool expected = true;
        if (ready_.compare_exchange_strong(expected, false)) {
            for (auto& agent : perfmon_agent_vec_) {
                if (agent.GetEventCtxNum() > 0) {
                    agent.DisableEvents();
                    agent.UnregisterEvents();
                }
            }
        }
        else {
            Dprintf(FRED("Close context failed due to context is not ready_: %p\n"), this);
            return -1;
        }
        Dprintf(FGRN("Perfmon collector closed..\n"));
        return 1;
//#endif
    }

    PerfmonCollector::~PerfmonCollector() {
        CloseContext();
    }

    std::map<std::pair<int, int>, std::vector<std::pair<int, int>>> PerfmonCollector::DebugAcquireEbpfCpuFdPairs() {
        std::map<std::pair<int, int>, std::vector<std::pair<int, int>>> output;

        auto& perfmon_agent_vec = perfmon_agent_vec_;
        auto cpu_onlines = util::GetCurrAvailableCPUList();

        // Register attach bpf to every perfmon collectors' opend event
        for (auto& online_id : cpu_onlines) {
            if (!(online_id >= 0 && online_id < perfmon_agent_vec.size())) {
                Dprintf(FRED("CPU number %d is invalid to init Ebpf perf event.\n"), online_id);
            }
            auto& percore_agent = perfmon_agent_vec[online_id];
            for (int ctx = 0; ctx < percore_agent.GetEventCtxNum(); ++ctx) {
                auto this_ctx = percore_agent.GetEventContext(ctx);
                for (int evt_id = 0; evt_id < this_ctx->event_num; ++evt_id) {
                    auto it = output.find({ctx, evt_id});
                    if (it == output.end()) {
                        output[{ctx, evt_id}] = {{online_id, this_ctx->fd[evt_id]}};
                    }
                    else {
                        output[{ctx, evt_id}].push_back({online_id, this_ctx->fd[evt_id]});
                    }
                }
            }
        }

        return output;
    }

}
