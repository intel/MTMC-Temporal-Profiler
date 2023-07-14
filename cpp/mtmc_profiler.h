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

#ifndef MTMC_MTMC_PROFILER_H
#define MTMC_MTMC_PROFILER_H

#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <list>
#include <memory>
#include<deque>
#include <stack>

#include "perfmon_collector.h"
#include "mtmc_temp_profiler.h"

namespace mtmc {

// TODO: In order to test mtmc + exporter, here, the size of single profile prefix is set to 256. Need to do sth if prefix is longer than this
#define SINGLE_DATA_PREFIX_BUFFER_SIZE 1024
    struct SingleProfile {
        // Prefix
        char prefix[SINGLE_DATA_PREFIX_BUFFER_SIZE];
        int64_t int_prefix;
        uint64_t hash_id;
        // Parent information
        ParamsInfo parent_info;
        // Thread id info
        int64_t tid;
        int32_t pthread_id;
        // Time and duration
        uint64_t start_ts;
        uint64_t end_ts;
        // PMC
        ReadResult rd_ret_start;
        uint64_t ret_start[16]; // TODO: Here is HARDCODE with 16, which is enough for current gen of Xeon processor. Consider dynamically change this value
        ReadResult rd_ret_end;
        uint64_t ret_end[16];
        int multiplex_idx;
        // FLAG bits
        union {
          struct {
              char has_start_info : 1,
              has_end_info : 1,
              has_topdown_info: 1,
              reserved : 5;
          } flag_bits;
          char flags;
        };
    };

    struct ThreadInfo {
        int64_t tid;
        int32_t pthread_id;
        util::IndexVector<SingleProfile>* storage_ptr;
        std::stack<size_t> data_tracer;
    };

    ThreadInfo& GetPerThreadInfo();

    template <typename T>
    class ThreadLocalSaver {
    public:
        ThreadLocalSaver() = default;

        T* GetThreadLocalData() {
            thread_local T t;
            return &t;
        }
    };

    template <typename T>
    class ThreadLocalStack {
    public:
        ThreadLocalStack() = default;

        std::deque<T>* GetThreadLocalStack() {
            thread_local std::deque<T> q;
            return &q;
        }
    };

    class Exporter {
    public:
        Exporter() = default;
        virtual ~Exporter() = default;

        virtual int Export(std::list<util::IndexVector<SingleProfile>>& profile_storage,
                           ProfilerSetting mtmc_setting) {
            return -1;
        }

        Exporter(const Exporter&) = delete;
        Exporter& operator=(const Exporter&) = delete;

    };

    class MTMCProfiler {
    public:
        MTMCProfiler();
        explicit MTMCProfiler(const std::string& config_addr);
        ~MTMCProfiler();

        /**
         * @name Init
         * @param none
         * @description do the init job for this profiler
         * @return 0 for success; < 0 for failure
         */
        int Init();

        /**
         * @name ExportThreadPoolInfo. Export thread pool information to the disk.
         * @param tids -- input,the list of tid
         * @return 0 for success; < 0 for failure
         */
        int ExportThreadPoolInfo(const std::vector<int64_t>& tids, const std::string& file_name);

        /**
         * @name Finish
         * @description Export the timeline to the path stored MTMC_LOG_EXPORT_PATH
         * @return 0 for success; < 0 for failure
         */
        int Finish();

        /**
         * @name Finish
         * @param file -- input, the path of output file
         * @description do the finish job and export the timeline to the output file
         */
        int Finish(const std::string& file);


        int Finish(Exporter& exporter);


        /**
         * @name Finish
         * @param file -- input, the path of output file
         * @param clear_when_done Clear storage space after finish.
         * @description do the finish job and export the timeline to the output file
         */
        int Finish(const std::string& file, bool clear_when_done);

        /**
         * @name GetParamsInfo
         * @description record record parent's tid and its information including  end time, thread it, cpu core, event information
         * @return 0 for success; < 0 for failure
         */
        ParamsInfo GetParamsInfo();

        /**
         * @name LogStart
         * @param params_info -- input, the params information, if params_info.parent_tid == -1, do the log job for first level
         * @param prefix -- input, prefix string for log information
         * @param hash_id -- input, a hash_id for this unique event
         * @description record parent's tid and its information including start time, thread it, cpu core, event information
         * @return 0 for success; < 0 for failure
         */
        int LogStart(ParamsInfo params_info, const std::string& prefix, uint64_t hash_id = 0);

        /**
         * @name LogStart
         * @param trace_info -- input, the parent(main task or inter-op task) information from GetParamsInfo function
         * @param prefix -- input, prefix string for log information. Does not affect data collection.
         * @description Should be called at the beginning of the subtask (intra-op task) to begin logging for the sub-task
         * @return 1 for success; < 0 for failure
         */
        int LogStart(TraceInfo trace_info, const std::string& prefix);

        /**
         * @name LogEnd
         * @return 0 for success; < 0 for failure
         */
        int LogEnd();

        /**
         * @name Close
         * @description Close the context and release the resources.
         * @return 2 for close the MTMCProfiler + underlying perfmon collector. 1 for close the MTMCProfiler only
         */
        int Close();

        /**
         * Disable the profiler globally. If the profiler is disabled, it will not capture any data with LogEnd or LogStart.
         * @return 1 and 0 for previous states (Enabled or disabled). -1 for failed
         */
        inline int GlobalProfilerDisable();

        /**
         * Enable the profiler globally.
         * @return 1 and 0 for previous states (Enabled or disabled). -1 for failed
         */
        inline int GlobalProfilerEnable();

        /**
         * Return the profiler's status.
         * @return 1 for enabled. 0 for disabled.
         */
        inline int IsEnabled();

        /**
         * Approximate number of byte used to store log data
         * @return size in byte
         */
        size_t StorageSize();

        /**
         * Clear the storage space for each thread at the next write operation
         * @return 1 for success. -1 for failed
         */
        int AsyncReuseStorageSpace();

        int AsyncClearStorageSpace();

        /**
         * Set global int prefix identifier. All data collected will record this global prefix identifier at that moment
         * @param int_prefix: a 64-bit int prefix value
         * @return 1 for success. -1 for failed
         */
        inline int SetGlobalIntPrefix(int64_t int_prefix);

        inline int UpdateCurrentCtx(const Context& ctx);

        inline Context* GetCurrentCtx();

        // based on ctx queue
        inline void PushCurrentCtx(const Context& ctx);
        inline void PopCurrentCtx();
        inline Context* CurrentCtx();
        inline int CtxQSize();

        int DirectPerCoreRead(PerCoreReadRet* data, bool is_start);

        void DebugPrint();

        void DebugPrintSingleProfile(SingleProfile* prof);

        std::shared_ptr<PerfmonCollector> DebugAcquirePerfmonCollector();

        const ProfilerSetting& GetProfilerSetting();

        // Sync variables for perfmon collector initialization
        std::atomic<int> Inited{};
        std::mutex InitMux{};

    private:
        std::string config_addr_;
        std::shared_ptr<PerfmonCollector> perfmon_collector_;
        ProfilerSetting mtmc_setting_{.perf_collect_topdown = false};

        // Init flag
        bool valid_{};

        // Enable-disable flag
        std::atomic<bool> collect_flag_{};

        // Global Int Prefix
        std::atomic<int64_t> global_int_prefix_{};

        // Collected data storage
        std::mutex mux_{};
        std::list<util::IndexVector<SingleProfile>> profile_storage_{};
        std::unordered_map<int64_t, util::IndexVector<SingleProfile>*> thread_storage_mapper{};

        // Context propagation
        ThreadLocalSaver<Context> ctx_saver{};

        ThreadLocalStack<Context> ctx_info;

        void RegisterPerThreadStorage(ThreadInfo* th_info, bool create_at_absence);
    };
}


#endif //MTMC_MTMC_PROFILER_H
