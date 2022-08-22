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

#ifndef MTMC_PERFMON_COLLECTOR_H
#define MTMC_PERFMON_COLLECTOR_H

#include <cstdio>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <atomic>

#include "env.h"
#include "util.h"
#include "perfmon_config.h"

namespace mtmc {

    static uint64_t ReadMmapPMC(void *addr, const ReadSetting& rd_setting);

    class PerfmonAgent {
    public:
        PerfmonAgent() = default;

        /**
         * Add a tester config to the tester.
         * Throw an exception when total number of event after adding is greater than GP_COUNTER
         * @param configs. InputConfig struct that contains a group of events
         * @return
         */
        int AddAttr(const InputConfig& configs);

        /**
         * Register all events that added by AddAttr func. Throw ErrorCodes if failed in perf_event_open or mmap
         * @return
         */
        int RegisterEvents();

        int UnregisterEvents();

        /**
         * ioctl enable all registered event group
         * @return
         */
        int EnableEvents();

        /**
         * ioctl reset all registered event group
         * @return
         */
        int ResetEvents();

        /**
         * ioctl reset all registered event group for every event fd
         * @return
         */
        int ResetEventsAllFd();

        /**
         * ioctl reset all registered event group for every event fd
         * @return
         */
        int EnableEventsAllFd();

        /**
         * ioctl reset all registered event group for every event fd
         * @return
         */
        int DisableEventsAllFd();

        /**
         * ioctl disable all registered event group
         * @return
         */
        int DisableEvents();

        /**
         * Ioctl to send request to perf.
         * @param ctxNum Index of the event context.
         * @param request Perf request, like enabling.
         * @return
         */
        int IoctlSingleEventCtx(int ctxNum, unsigned long int request);

        /**
         * Return the specific Event context
         * @param ctxNum Index of the event context
         * @return
         */
        EventCtx* GetEventContext(int ctxNum);

        /**
         * Return the specific tester config for the event context
         * @param ctxNum Index of the event context
         * @return
         */
        InputConfig* GetTesterConfig(int ctxNum);

        int GetEventCtxNum();

        int ReadCounter(int print = 1);

        /**
         * Check all the event context to see if any ctx need to reset ( time since last reset > min_reset_intrvl)
         * @return 1 for success, -1 for failed
         */
        int CheckAndResetEventCtx();

    private:
        int num_events_here_ = 0;
        std::vector<InputConfig> cfg_vec_;
        std::vector<EventCtx> ctx_vec_;
        uint64_t pmc_result_[GP_COUNTER]{};

        static int perf_event_open(struct perf_event_attr *attr, pid_t pid, int cpu, int group_fd, unsigned long flags);

    };

    struct ReadResult {
        int num_event;
        uint32_t core_id;
        uint32_t prefix;
    };

    /**
     *  PerfmonCollector class will set up corresponding registers and provide apis to read PMC
     */
    class PerfmonCollector {
    public:
        PerfmonCollector();

        ~PerfmonCollector();
        /**
         * Initialize perfmon collector context with groups of config. It will use perf kernel api to init the pmc and
         * mmap the perf_mmap_pages.
         * @param input_config: vector of configs from PerfmonConfig. Each config is a group of pmcs
         * @param num_core: List of cores that will be monitored.
         * a range of cores
         * @return 1 for success
         */
        int InitContext(std::vector<InputConfig> &input_config, std::vector<int> num_core);

        int CloseContext();

        /**
         * The function will read all event context's pmc on current core and store it in the uint64_t ret array.
         * @param ret: The array that stores the pmc results.
         * @param is_start: is_start is true means the PerCoreRead will do some reset and init function to make sure the
         * @param rd_ret: ReadResult. Stores the information like core id and event number for this reading.
         * correctness ot the final result.
         * @return 1 for success
         */
        int PerCoreRead(uint64_t* ret, bool is_start, ReadResult* rd_ret);

        void DebugPrint();

    private:
        std::vector<std::vector<void*>> perf_mmap_pages_vec_;
        std::vector<PerfmonAgent> perfmon_agent_vec_;
        std::atomic<bool> ready_;
    };

}


#endif //MTMC_PERFMON_COLLECTOR_H
