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

#include "mtmc_profiler.h"
#include "mtmc_temp_profiler.h"

namespace mtmc {

    // -------------------------------------- MTMCProfiler ----------------------------------------

    std::atomic<int> MTMCProfiler::Inited(0);
    std::mutex MTMCProfiler::InitMux;
    std::shared_ptr<PerfmonCollector> MTMCProfiler::perfmon_collector_;
    ProfilerSetting MTMCProfiler::mtmc_setting_{.perf_collect_topdown = false};

    MTMCProfiler::MTMCProfiler() {
        auto env_config = getenv("MTMC_CONFIG");
        if (env_config) {
            config_addr_ = std::string(env_config);
            Dprintf(FBLU("Store config address %s\n"), env_config);
        }
        else {
            config_addr_.clear();
            Dprintf(FRED("No environment variable MTMC_CONFIG found, so perfmon metrics are disabled\n"));
        }
        valid_ = false;
    }

    MTMCProfiler::MTMCProfiler(const std::string& config_addr) {
        config_addr_ = config_addr;
        Dprintf(FBLU("Store config address %s\n"), config_addr.c_str());
        valid_ = false;
    }

    int MTMCProfiler::Init() {
        /* Initialization includes programing msr. So multiple instance running in parallel will contaminate each other
         * User can pass env var MTMC_PROF_DISABLE to manually disable a profiler in a process */
        auto disable_flag = getenv("MTMC_PROF_DISABLE");
        if (disable_flag != NULL) {
            std::string disable_s = std::string(disable_flag);
            if (disable_s.length() == 1 || disable_s.length() == 4) {
                if (!disable_s.compare("1") || !disable_s.compare("true")) {
                    Dprintf(FRED("MTMC Profiler for tid %lld will not be inited due to disabled by ENV\n"), Env::GetKtid());
                    return -1;
                }
            }
        }

        /*if (disable_flag != NULL && (!strncmp(disable_flag, "1",1) || !strncmp(disable_flag, "true",4))) {
            Dprintf(FRED("MTMC Profiler for tid %lld will not be inited due to disabled by ENV\n"), Env::GetKtid());
            return -1;
        }*/

        /* Basic Rule: There can be multiple mtmc_profilers instance in the process, but all of them share 1 perfmon_collector and
         * only 1 mtmc_profiler can init the collector. */
        std::lock_guard<std::mutex> lock(InitMux);
        if (Inited.load() < 1) {
            Dprintf(FGRN("This profiler get the perfmon collector init mux and will initialize it.\n"));

            std::vector<InputConfig> cfg;
            auto stat = PerfmonConfig::ReadConfig(config_addr_, &cfg, &mtmc_setting_);
            if (stat != 1) {
                Dprintf(FRED("Initialization failed. Can not read config file from the given address\n"));
                return -1;
            }
            // TODO: HARDCODE to add perfmon metric here. For ICX below machine, this filed should through an error, later should add condition to check
            if (mtmc_setting_.perf_collect_topdown) {
                Dprintf(FCYN("Will read topdown metrics\n"));
                PerfmonConfig::PerfMetricConfig(&cfg, 1);
            }
            perfmon_collector_ = std::make_shared<PerfmonCollector>();
            if (perfmon_collector_->InitContext(cfg, util::GetCurrAvailableCPUList()) == -1) {
                perfmon_collector_.reset();
                Dprintf(FRED("Initialization failed due to underlying perfmon collector failed initialization\n"));
                return -1;
            }
            Inited.fetch_add(1);
            Dprintf(FGRN("MTMC Profiler has been initialized\n"));
        }
        else {
            if (!valid_) Inited.fetch_add(1);
            Dprintf(FGRN("MTMC Profiler has ALREADY been initialized\n"));
        }
        valid_ = true;
        return 1;
    }

    ParamsInfo MTMCProfiler::GetParamsInfo() {
        if (!valid_) return ParamsInfo{};
        ParamsInfo ret{};
        // Here implements thread_local storage for the kernel thread id. In a thread-pool scenario, this will help
        // reduce the num of syscall, but may waste resource if the thread will destruct every time
        thread_local int64_t ktid = -1;
        thread_local int32_t pthread_tid = -1;
        if (ktid == -1) {
            ktid = Env::GetKtid();
            DDprintf("GetParamsInfo first init kernel tid to thread storage. Tid: %lld\n", ktid);
        }
        if (pthread_tid == -1) {
            pthread_tid = Env::GetPthreadid();
            DDprintf("GetParamsInfo first init pthread tid to thread storage. Tid: %lld\n", pthread_tid);
        }
        // TODO: NULL ptr test
        ret.parent_tid = ktid;
        ret.parent_pthread_id = pthread_tid;
        ret.task_sched_time = Env::GetClockTimeNs();
        return ret;
    }

    int MTMCProfiler::LogStart(ParamsInfo params_info, std::string prefix) {
        if (!valid_) return -1;
        thread_local ThreadInfo th_info = ThreadInfo{.tid = -1, .pthread_id = -1, .storage_ptr=nullptr};
        if (th_info.tid == -1) {
            // Store the thread's kernel thread id
            th_info.tid = Env::GetKtid();
            th_info.pthread_id = Env::GetPthreadid();
            RegisterPerThreadStorage(&th_info, true);
        }
        DDprintf("LogStart {%lld,%p}, size: %llu\n", th_info.tid, th_info.storage_ptr, th_info.storage_ptr->size());
        th_info.storage_ptr->push_back(SingleProfile());
        SingleProfile* log_info = &th_info.storage_ptr->back();

        log_info->prefix = std::move(prefix);
        log_info->parent_info = params_info;
        log_info->tid = th_info.tid;
        log_info->pthread_id = th_info.pthread_id;

        log_info->start_ts = Env::GetClockTimeNs();
        auto status = perfmon_collector_->PerCoreRead(true, log_info->ret_start, &log_info->rd_ret_start);
        if (status == -1) {
            DDprintf(FRED("Failed perfmon_collector per core read\n"));
        }

        log_info->has_start_info = true;
        return 1;
    }

    int MTMCProfiler::LogEnd() {
        if (!valid_) return -1;
        thread_local ThreadInfo th_info = ThreadInfo{.tid = -1, .pthread_id = -1, .storage_ptr=nullptr};
        if (th_info.tid == -1) {
            // Store the thread's kernel thread id
            th_info.tid = Env::GetKtid();
            th_info.pthread_id = Env::GetPthreadid();
        }
        if (th_info.storage_ptr == nullptr) {
            RegisterPerThreadStorage(&th_info, false);
        }
        // Sanity checks
        if (th_info.storage_ptr == nullptr || th_info.storage_ptr->empty()) {
            Dprintf(FRED("LogEnd detect an empty storage space, it could means that LogEnd is called before LogStart\n"));
            return -1;
        }
        DDprintf("LogEnd {%lld,%p}, size: %llu\n", th_info.tid, th_info.storage_ptr, th_info.storage_ptr->size());
        SingleProfile* log_info = &th_info.storage_ptr->back();
        // Sanity checks again
        if (!log_info->has_start_info) {
            Dprintf(FRED("LogEnd detect an empty start log info, it means that LogEnd is called before LogStart\n"));
            return -1;
        }

        log_info->end_ts = Env::GetClockTimeNs();
        auto status = perfmon_collector_->PerCoreRead(false, log_info->ret_end, &log_info->rd_ret_end);
        if (status == -1) {
            DDprintf(FRED("Failed perfmon_collector per core read\n"));
        }

        return 1;
    }

    int MTMCProfiler::Finish(const std::string& file) {

        if (!valid_) {
            Dprintf(FRED("The mtmc profiler is not valid. So finish function will export nothing\n"));
            return -1;
        }

        std::ofstream resultfs;
        resultfs.open(file, std::ios::out);
        if (!resultfs.good()) {
            Dprintf("Open file failed: %s\n", file.c_str());
            return -1;
        }
        for (auto & vec : profile_storage_) {
            for (auto & profile : vec) {
                /* Here are some invalid data conditions */
                if (profile.rd_ret_start.num_event != profile.rd_ret_end.num_event) {
                    Dprintf(FRED("Event number mismatch\n"));
                    continue;
                }
                if (mtmc_setting_.perf_collect_topdown && (profile.rd_ret_start.num_event - 2 < 0)) {
                    Dprintf(FRED("Topdown metric and event number mismatch\n"));
                    continue;
                }
                resultfs << profile.tid << ",";
                resultfs << (uint32_t)(profile.pthread_id) << ",";
                resultfs << profile.start_ts << ",";
                resultfs << profile.end_ts << ",";
                resultfs << profile.parent_info.parent_tid << ",";
                resultfs << (uint32_t)(profile.parent_info.parent_pthread_id) << ",";
                resultfs << profile.parent_info.task_sched_time << ",";

                // Export topdown metrics separately
                auto num_event = profile.rd_ret_start.num_event;
                if (mtmc_setting_.perf_collect_topdown) {
                    num_event-= 3;
                    resultfs << profile.ret_start[num_event] <<  "_" << profile.ret_start[num_event+1] << "_" << profile.ret_start[num_event+2] << "_"
                             << profile.ret_end[num_event] << "_" << profile.ret_end[num_event+1] << "_" << profile.ret_end[num_event+2];
                }
                resultfs << ",";

                // Export events
                resultfs << profile.rd_ret_start.num_event << "_" << profile.rd_ret_start.core_id << "_" << profile.rd_ret_start.prefix << ",";
                for (int i = 0; i < num_event; ++i) {
                    resultfs << profile.ret_start[i];
                    if (i != num_event - 1) resultfs << "_";
                    else resultfs << ",";
                }
                resultfs << profile.rd_ret_end.num_event << "_" << profile.rd_ret_end.core_id << "_" << profile.rd_ret_end.prefix << ",";
                for (int i = 0; i < num_event; ++i) {
                    resultfs << profile.ret_end[i];
                    if (i != num_event - 1) resultfs << "_";
                    else resultfs << ",";
                }
                resultfs << std::endl;
            }
        }
        resultfs.close();
        Dprintf("Export done\n");
        return 1;
    }

    int MTMCProfiler::Close() {
        if (this->valid_) {
            Dprintf(FGRN("Close MTMC Profiler. Exist %d profiler uses live perfmon_collector\n"), Inited.load()-1);
            this->valid_ = false;
            std::lock_guard<std::mutex> lock(InitMux);
            if (Inited.fetch_sub(1) <= 1) {
                if (perfmon_collector_) {
                    perfmon_collector_.reset();
                    return 2;
                }
            }
        }
        return 1;
    }

    int MTMCProfiler::ExportThreadPoolInfo(const std::vector<int64_t>& tids, const std::string& file_name) {
        auto export_addr = getenv("MTMC_THREAD_EXPORT");
        if (export_addr) {

            for (auto& tid : tids) {
                if (tid < 0) {
                    Dprintf("Failed export tid due to found negative tid\n");
                    return -1;
                }
            }

//            auto name = std::to_string(reinterpret_cast<int64_t>(this));
            auto name = file_name;
            auto addr = util::PathJoin(std::string(export_addr), name);
            std::ofstream exportfs;
            exportfs.open(addr, std::ios::out);

            if (!exportfs.good()) {
                Dprintf("Export failed due to file given by MTMC_THREAD_EXPORT is invalid\n");
                return -1;
            }

            /* Export Thread pool information */
            for (int th_tid = 0; th_tid < tids.size(); ++th_tid) {
                exportfs << th_tid << "," << tids[th_tid] << std::endl;
            }
            exportfs.close();
            Dprintf("Export thread pool information to %s\n", addr.c_str());

        }
        else {
            Dprintf(FRED("Thread pool information will not be exported since no MTMC_THREAD_EXPORT environ is found\n"));
            return -1;
        }
        return 1;
    }

    void MTMCProfiler::DebugPrint() {
        Dprintf(FCYN("======== Profiler Infos =========\n"));

        Dprintf(FCYN("profile_storage_:\n"));
        auto ps_size = profile_storage_.size();
        Dprintf("Vector size: %lu\n", ps_size);
        int i = 0;
        for (auto itr = profile_storage_.begin(); itr != profile_storage_.end(); ++itr) {
            auto inner_size = itr->size();
            Dprintf(FMAG("profiler_storage_[%d] size: %lu, ptr: %p\n"), i, inner_size, &(*itr));
            for (int j = 0; j < inner_size; ++j) {
                DebugPrintSingleProfile(&(*itr)[j]);
            }
            ++i;
        }

        Dprintf(FCYN("thread_storage_mapper_:\n"));
        auto tsm_size = thread_storage_mapper.size();
        for (auto itr = thread_storage_mapper.begin(); itr != thread_storage_mapper.end(); ++itr) {
            Dprintf("{%lld,%p}\n", itr->first, itr->second);
        }
        Dprintf(FCYN("============= End ===============\n"));
    }

    void MTMCProfiler::DebugPrintSingleProfile(SingleProfile* prof) {
        Dprintf("---- Single profile ----\n");
        Dprintf("Parent info: %ld, %lu\n", prof->parent_info.parent_tid, prof->parent_info.task_sched_time);
        Dprintf("Thread info: %ld, %lu, %lu\n", prof->tid, prof->start_ts, prof->end_ts);
        Dprintf("Start info: %d, %d, %d\n", prof->rd_ret_start.core_id, prof->rd_ret_start.prefix,prof->rd_ret_start.num_event);
        Dprintf("TMAM info: ");
        for (int i = 0; i < prof->rd_ret_start.num_event; ++i) {
            printf("%ld,", prof->ret_start[i]);
        }
        printf("\n");
        Dprintf("End info: %d, %d, %d\n", prof->rd_ret_end.core_id, prof->rd_ret_end.prefix,prof->rd_ret_end.num_event);
        Dprintf("TMAM info: ");
        for (int i = 0; i < prof->rd_ret_end.num_event; ++i) {
            printf("%ld,", prof->ret_end[i]);
        }
        printf("\n");
        Dprintf("---- Single profile end ----\n");
    }

    MTMCProfiler::~MTMCProfiler() {
        MTMCProfiler::Close();
    }

    // ----------------------------------- Private ----------------------------------------

    void MTMCProfiler::RegisterPerThreadStorage(ThreadInfo* th_info, bool create_at_absence) {
        std::lock_guard<std::mutex> lock(mux_);
        auto storage_ptr_itr = thread_storage_mapper.find(th_info->tid);
        if (storage_ptr_itr == thread_storage_mapper.end()) {
            if (create_at_absence) {
                // Init this thread's storage vector. Push back an empty vector<SingleProfile>
                profile_storage_.emplace_back(0);
                th_info->storage_ptr = &(profile_storage_.back());
                th_info->storage_ptr->reserve(1024*1024*100); // TODO: Use list or Vector+reserve?
                // Add the thread info to the map
                thread_storage_mapper.insert({th_info->tid, th_info->storage_ptr});
//            Dprintf("Create new {%ld,%p}\n",th_info->tid, th_info->storage_ptr);
            }
            else {
                th_info->storage_ptr = nullptr;
//                Dprintf("Create at absense is false\n");
            }

        }
        else {
            th_info->storage_ptr = storage_ptr_itr->second;
//            Dprintf("Get existing {%ld,%p}\n",th_info->tid, storage_ptr_itr->second);
        }
    }

    // ----------------------------------- Interface --------------------------------------

    MTMCTemprolProfiler::MTMCTemprolProfiler() {
        profiler_impl = std::make_shared<MTMCProfiler>();
    }

    MTMCTemprolProfiler::MTMCTemprolProfiler(const std::string &config_addr) {
        profiler_impl = std::make_shared<MTMCProfiler>(config_addr);
    }

    int MTMCTemprolProfiler::Init() {
        return profiler_impl->Init();
    }

    int MTMCTemprolProfiler::ExportThreadPoolInfo(const std::vector<int64_t> &tids) {
        return profiler_impl->ExportThreadPoolInfo(tids, std::to_string(reinterpret_cast<int64_t>(this)));
    }

    int MTMCTemprolProfiler::Finish(const std::string &file) {
        return profiler_impl->Finish(file);
    }

    ParamsInfo MTMCTemprolProfiler::GetParamsInfo() {
        return profiler_impl->GetParamsInfo();
    }

    int MTMCTemprolProfiler::LogStart(ParamsInfo params_info, const std::string& prefix) {
        return profiler_impl->LogStart(params_info, prefix);
    }

    int MTMCTemprolProfiler::LogEnd() {
        return profiler_impl->LogEnd();
    }

    int MTMCTemprolProfiler::Close() {
        return profiler_impl->Close();
    }

    MTMCTemprolProfiler::~MTMCTemprolProfiler() {}

    int64_t MTMCTemprolProfiler::GetKtid() {
        return Env::GetKtid();
    }

}
