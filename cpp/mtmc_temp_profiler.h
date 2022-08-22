//
// Created by yuqiuzha on 8/10/2022.
//

#ifndef MTMC_MTMC_TEMP_PROFILER_H
#define MTMC_MTMC_TEMP_PROFILER_H

namespace mtmc {

    class MTMCProfiler;
    struct ParamsInfo {
        // Thread id info
        int64_t parent_tid;
        int32_t parent_pthread_id;
        // Time and duration
        uint64_t task_sched_time;
    };

    class MTMCTemprolProfiler {
    public:

        /**
         * @description: MTMC temprol perfiler instance.
         */
        MTMCTemprolProfiler();
        explicit MTMCTemprolProfiler(const std::string& config_addr);
        ~MTMCTemprolProfiler();

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
         * @param params_info -- input, the parent(main task or inter-op task) information from GetParamsInfo function
         * @param prefix -- input, prefix string for log information. Does not affect data collection.
         * @description Should be called at the beginning of the subtask (intra-op task) to begin logging for the sub-task
         * @return 1 for success; < 0 for failure
         */
        int LogStart(ParamsInfo params_info, const std::string& prefix);

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
         * @name GetKtid
         * @description Run system call to return a 64bit kernel thread id
         * @return return int64_t kernel thread id
         */
        static int64_t GetKtid();

        MTMCTemprolProfiler(const MTMCTemprolProfiler&) = delete;
        MTMCTemprolProfiler& operator=(const MTMCTemprolProfiler&) = delete;

    private:
        std::shared_ptr<MTMCProfiler> profiler_impl;
    };
}

#endif //MTMC_MTMC_TEMP_PROFILER_H
