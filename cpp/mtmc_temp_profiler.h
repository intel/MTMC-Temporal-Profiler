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

#ifndef MTMC_MTMC_TEMP_PROFILER_H
#define MTMC_MTMC_TEMP_PROFILER_H

#include <memory>
#include <string>
#include <vector>

namespace mtmc {

    class MTMCProfiler;
    struct ParamsInfo {
        // Thread id info
        int64_t parent_tid;
        int32_t parent_pthread_id;
        // Time and duration
        uint64_t task_sched_time;
        // Context
        uint64_t parent_ctx_hash_id;
    };

    // trace/span information
    struct TraceInfo {
        // id for this tracing
        uint64_t trace_id;
        // id for the parent span
        uint64_t parent_id;
        // id for current span
        uint64_t current_id;
        // name for current span
        std::string name;
        // op for current span
        std::string op;

        // tid info
        int64_t tid;
        // pthread if info
        int32_t pthread_id;
        // parent_tid info
        int64_t parent_tid;
        // parent pthread if info
        int32_t parent_pthread_id;
        // inter op or not
        bool inter_op;

        //inputs for this span
        std::vector<std::string> inputs;
    };

    struct PerCoreReadRet {
        int num_event;
        uint32_t core_id;
        uint64_t readings[16];
    };

    struct Context {
        bool is_default = true;
        int level = 0;
        std::string name = "Default Name";
        std::string op;
        uint64_t ctx_hash_id;
        uint64_t tracing_hash_id;
	    // should set seq offset;
	    bool set_offset_seq;
        uint64_t offset_seq;
    };

    class MTMCTemprolProfiler {
    public:
        /**
         * @name Init
         * @param none
         * @description Init mtmc temprol profiler instance. There can be multiple profiler in a process. But can not have
         * multiple process init profiler together. The instance will not init if environ MTMC_PROF_DISABLE is set to 1 or true
         * @return 1 for success; < 0 for failure
         */
        int Init();

        /**
         * @name ExportThreadPoolInfo
         * @param tids -- input,the list of tid
         * @description Export thread pool information to the disk. The target file is stored at the address from
         * environment variable [MTMC_THREAD_EXPORT]. The file name is the pointer to the current profiler
         * @return 1 for success; < 0 for failure
         */
        int ExportThreadPoolInfo(const std::vector<int64_t>& tids);

        /**
         * @name Finish
         * @param file -- input, the path of the output file
         * @description Export all logging data to a file on disk.
         * @return 1 for success, < 0 for failure
         */
        int Finish(const std::string& file);

        /**
         * @name GetParamsInfo
         * @description Record current thread's tid, pthread id and time as scheduling time
         * @return ParamsInfo struct. If failed, all data in the stuct is 0.
         */
        ParamsInfo GetParamsInfo();

        /**
         * @name LogStart
         * @param prefix -- input, prefix string for log information. Does not affect data collection.
         * @description Should be called at the beginning of a subtask
         * @return 1 for success; < 0 for failure
         */
        int LogStart(const std::string& prefix);

        /**
         * @name LogStart
         * @param params_info -- input, the parent(main task or inter-op task) information from GetParamsInfo function
         * @param prefix -- input, prefix string for log information. Does not affect data collection.
         * @description Should be called at the beginning of the subtask (intra-op task) to begin logging for the sub-task
         * @return 1 for success; < 0 for failure
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
         * @description Should be called at the the end of the subtask (intra-op task).
         * @return 1 for success; < 0 for failure
         */
        int LogEnd();

        /**
         * @name Close
         * @description Close the mtmc profiler context and release the resources.
         * @return 1 for success but not releasing PMC registers; 2 for success and release PMC registers.
         */
        int Close();

        /**
         * Disable the profiler globally. If the profiler is disabled, it will not capture any data with LogEnd or LogStart.
         * @return 1 and 0 for previous states (Enabled or disabled). -1 for failed
         */
        int GlobalProfilerDisable();

        /**
         * Enable the profiler globally.
         * @return 1 and 0 for previous states (Enabled or disabled). -1 for failed
         */
        int GlobalProfilerEnable();

        /**
         * Return the profiler's status.
         * @return 1 for enabled. 0 for disabled.
         */
        int IsEnabled();

        /**
         * Set global int prefix identifier. All data collected will record this global prefix identifier at that moment
         * @param int_prefix: a 64-bit int prefix value
         * @return 1 for success. -1 for failed
         */
        int SetGlobalIntPrefix(int64_t int_prefix);

        /**
         * Approximate number of byte used to store log data
         * @return size in byte
         */
        size_t StorageSize();

        /**
         * Clear the storage for each thread at the next write operation. The clear will NOT release the memory
         * of the storage. To clear and release the memory, use AsyncClearStorageSpace
         * @return 1 for success. -1 for failed
         */
        int AsyncReuseStorageSpace();

        /**
         * Clear the storage for each thread and release their memory at the next write operation. (eg. LogStart or LogEnd)
         * @return 1 for success. -1 for failed
         */
        int AsyncClearStorageSpace();

        /**
         * Read current core's PMC directly and stored to data.
         * @param data: pointer to PerCoreReadRet to store the PMC reading
         * @param is_start: Set true will check for PMC reset.
         * @return 1 for success. -1 for failed
         */
        int DirectPerCoreRead(PerCoreReadRet* data, bool is_start);

        /**
         * Update current thread's context to the input
         * @param ctx: Input context to be updated
         * @return 1 for success, -1 for failed
         */
        int UpdateCurrentCtx(const Context& ctx);

        /**
         * Return current thread's context.
         * @return Pointer to current thread's context.
         */
        Context* GetCurrentCtx();

        // based on ctx queue
        /**
         * Push ctx into thread's queue.
         */
        void PushCurrentCtx(const Context& ctx);

        /**
         * Pop the ctx from the thread's queue.
         *
         */
        void PopCurrentCtx();

        /**
         * get the current ctx.
         * @return Pointer to current thread's context.
         */
        Context* CurrentCtx();

        /**
         * get the ctx queue's size.
         * @return the size of the ctx queue.
         */
        int CtxQSize();

        /**
         * For Debug use. Return the underlying MTMC profiler reference.
         * @return Reference to the underlying MTMC profiler.
         */
        MTMCProfiler& DebugGetProfilerRef() {
            return *profiler_impl;
        };

        /**
         * @name GetKtid
         * @description Run system call to return a 64bit kernel thread id
         * @return return int64_t kernel thread id
         */
        static int64_t GetKtid();

        static MTMCTemprolProfiler& getInstance();

        MTMCTemprolProfiler(const MTMCTemprolProfiler&) = delete;
        MTMCTemprolProfiler& operator=(const MTMCTemprolProfiler&) = delete;

    private:
        /**
         * @description: MTMC temprol profiler instance.
         */
        MTMCTemprolProfiler();
        explicit MTMCTemprolProfiler(const std::string& config_addr);
        ~MTMCTemprolProfiler();

        std::shared_ptr<MTMCProfiler> profiler_impl;
    };
}

#endif //MTMC_MTMC_TEMP_PROFILER_H
