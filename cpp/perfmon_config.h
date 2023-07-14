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

#ifndef MTMC_PERFMON_CONFIG_H
#define MTMC_PERFMON_CONFIG_H

#include <linux/perf_event.h>
#include <vector>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <set>

#include "nlohmann/json.hpp"
#include "env.h"
#include "util.h"

#include <ext/stdio_filebuf.h>

/// x86 kernel configuration for perfmon. From Linux kernel 5.16 source code
extern "C" {
union x86_pmu_config {
    struct {
        uint64_t event: 8,
                umask: 8,
                usr: 1,
                os: 1,
                edge: 1,
                pc: 1,
                interrupt: 1,
                __reserved1: 1,
                en: 1,
                inv: 1,
                cmask: 8,
                event2: 4,
                __reserved2: 4,
                go: 1,
                ho: 1;
    } bits;
    uint64_t value;
};

#define X86_CONFIG(args...) ((union x86_pmu_config){.bits = {args}}).value

}

namespace mtmc {

    uint64_t X86Config(int event, int umask, int inv, int cmask, int edge);

    struct ReadSetting {
        int32_t min_reset_intrvl_ns; // Interval between resetting the register. -1 means no reset at all
        bool add_offset; // Add offsets when reading the pmc
        bool sign_ext; // Do sign extend shift when reading the pmc
    };

    struct InputConfig {
        int event_num;
        std::string names[GP_COUNTER];
        perf_event_attr attr_arr[GP_COUNTER];
        int cpu_arr[GP_COUNTER];
        int pid_arr[GP_COUNTER];
        int fd[GP_COUNTER];
        std::string group_name;
        ReadSetting rd_setting;
        uint64_t multiplex_intv;
    };

    struct EventCtx {
        int event_num;
        int fd[GP_COUNTER];
        void* addr[GP_COUNTER];
        int id[GP_COUNTER];
        ReadSetting rd_setting;
        uint64_t last_reset_tsc;     // The tsc that this eventctx was reset last time
    };

    struct ProfilerSetting {
        util::CFG_FILE_TYPE cfg_type;

        /* Collect Topdown.slots and Topdown.metric PMC */
        bool perf_collect_topdown;

        /* Use eBPF to collect context switch PMC */
        bool ebpf_collect_ctxsc_pmc;

        /* Constant variables required by the config */
        std::set<std::string> cnst_var;

        /* ExportMode */
        int export_mode;

        /* 64bit unique hash value for a given run. Only required by OTLE-Jaeger */
        uint64_t trace_hash;

        /* Configuration file id. Used for per process event mux */
        int configs_id;
    };

    class PerfmonConfig {
    public:
        PerfmonConfig() = default;

        /**
         * Read txt config into vector of InputConfig that can be feed into perfmon_collector
         * @param file_path: Path to the config
         * @param config_vec: Ptr to the config vector that stores the input config
         * @return 1 for successful, otherwise failed
         */
        static int ReadConfig(const std::string& file_path, std::vector<InputConfig>* config_vec, ProfilerSetting* mtmc_setting);

        /**
         * Read json config into vector of InputConfig that can be feed into perfmon_collector
         * @param file_path: Path to the config
         * @param config_vec: Ptr to the config vector that stores the input config
         * @param mtmc_setting: Ptrto the Global MTMC profiler setting
         * @return 1 for success, otherwise failed
         */
        static int ReadConfigJson(const std::string& file_path, std::vector<InputConfig>* config_vec, ProfilerSetting* mtmc_setting);

        /**
         * Generate ICX/12thGenCore Topdown.metric + Topdown.slot reading config
         * @param config_vec: Output config vector
         * @param reset_intrvl_ns: nano second intervals between clearing these two pmc, since clear op is expensive (need to go through kernel)
         * @return 1 for successful, otherwise failed
         */
        static int PerfMetricConfig(std::vector<InputConfig>* config_vec, int32_t reset_intrvl_ns);

    };

}

#endif //MTMC_PERFMON_CONFIG_H
