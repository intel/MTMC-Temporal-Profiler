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

#include "perfmon_collector.h"
#include "mtmc_temp_profiler.h"

namespace mtmc {

    struct SingleProfile {
        // Prefix
        std::string prefix;
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
        // FLAG bits
        bool has_start_info;
    };

    struct ThreadInfo {
        int64_t tid;
        int32_t pthread_id;
        std::vector<SingleProfile>* storage_ptr;
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
         * @param file -- input, the path of output file
         * @description do the finish job and export the timeline to the output file
         */
        int Finish(const std::string& file);

        /**
         * @name GetParamsInfo
         * @param params_info -- input, the params information, if params_info.parent_tid == -1, do the log job for first level
         * @param string prefix -- input, prefix string for log inforamtion
         * @description record record parent's tid and its information including  end time, thread it, cpu core, event information
         * @return 0 for success; < 0 for failure
         */
        ParamsInfo GetParamsInfo();

        /**
         * @name LogStart
         * @param params_info -- input, the params information, if params_info.parent_tid == -1, do the log job for first level
         * @param prefix -- input, prefix string for log information
         * @description record parent's tid and its information including start time, thread it, cpu core, event information
         * @return 0 for success; < 0 for failure
         */
        int LogStart(ParamsInfo params_info, std::string prefix);

        /**
         * @name LogEnd
         * @param params_info -- input, the params information, if params_info.parent_tid == -1, do the log job for first level
         * @return 0 for success; < 0 for failure
         */
        int LogEnd();

        /**
         * @name Close
         * @description Close the context and release the resources.
         * @return 2 for close the MTMCProfiler + underlying perfmon collector. 1 for close the MTMCProfiler only
         */
        int Close();

        void DebugPrint();

        void DebugPrintSingleProfile(SingleProfile* prof);

        // Sync variables for perfmon collector initialization
        static std::atomic<int> Inited;
        static std::mutex InitMux;

    private:
        std::string config_addr_;
        static std::shared_ptr<PerfmonCollector> perfmon_collector_;
        static ProfilerSetting mtmc_setting_;

        // Init flag here to make sure only 1 profiler can be
        bool valid_{};

        // Collected data storage
        std::mutex mux_;
        std::list<std::vector<SingleProfile>> profile_storage_;
        std::unordered_map<int64_t, std::vector<SingleProfile>*> thread_storage_mapper;

        void RegisterPerThreadStorage(ThreadInfo* th_info, bool create_at_absence);
    };

}


#endif //MTMC_MTMC_PROFILER_H
