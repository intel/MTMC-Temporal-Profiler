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
#include <cstdlib>
#include <cstdio>

#include "mtmc_profiler.h"
#include "mtmc_temp_profiler.h"
#include "test_util.h"
#include "linux/sched.h"
#include "guard_sampler.h"

using namespace mtmc;

/**
 * Test config reader that reads txt input config and convert to Configs that feed to PerfmonCollector
 */
void TestPerfmonConfigReader() {
    std::vector<InputConfig> input_config;
    mtmc::ProfilerSetting mtmc_setting{};
    PerfmonConfig::ReadConfig("/home/intel/work/mtmc/cpp/tests/configs/configExampleCaches.txt", &input_config, &mtmc_setting);
    Dprintf("Done Test Perfmon config reader\n");
}

/**
 * Test read PerfMetric and PerfSlots
 */
void TestPerfMetricRead() {

    std::vector<InputConfig> input_config;
    PerfmonConfig::PerfMetricConfig(&input_config, 100);

    PerfmonCollector pfc;
    pfc.InitContext(input_config, util::GetCurrAvailableCPUList());

    // Create jobs
    testutl::Jobs jobs;

    jobs.DoJob(testutl::Jobs::MEMCPY, 3000);

    // Read Once at the beginning
    uint32_t core_id, prefix;
    Env::GetCoreId(&core_id, &prefix);

    Dprintf("Current core: %d, %d\n", core_id, prefix);
    uint64_t ret_start[GP_COUNTER];
    uint64_t ret_end[GP_COUNTER];
    testutl::TopDownRet cal_start{}, cal_end{};

    ReadResult rd_ret;
    auto status = pfc.PerCoreRead(true, ret_start, &rd_ret);
    Dprintf("Number of events: %d\n", rd_ret.num_event);
    Dprintf("Slot: %lld\n", ret_start[0]);
    Dprintf("Metric: %lld\n", ret_start[1]);
    cal_start.slots = ret_start[0];
    cal_start.metric = ret_start[1];

    // Do job
    jobs.DoJob(testutl::Jobs::MULTIPLY, 1);

    // Read again
    status = pfc.PerCoreRead(false, ret_end, &rd_ret);
    Dprintf("Number of events: %d\n", rd_ret.num_event);
    Dprintf("Slot: %lld\n", ret_end[0]);
    Dprintf("Metric: %lld\n", ret_end[1]);
    cal_end.slots = ret_end[0];
    cal_end.metric = ret_end[1];

    for (int i = 0; i < rd_ret.num_event; ++i) {
        Dprintf("Difference: %lld\n", ret_end[i]-ret_start[i]);
    }

    testutl::calDelta(cal_start, cal_end);

    Dprintf("Done Test Perfmon collector\n");
}

void TestPerfmonCollector() {

    // Read example config
    std::vector<InputConfig> input_config;
    mtmc::ProfilerSetting mtmc_setting{};
    PerfmonConfig::ReadConfig("/home/intel/work/mtmc/cpp/tests/configs/configExampleCaches.txt", &input_config, &mtmc_setting);

    // Initialize context
    PerfmonCollector pfc;
    pfc.InitContext(input_config, util::GetCurrAvailableCPUList());
    Dprintf("Done initialization perfmon collector\n");

    // Create jobs
    testutl::Jobs jobs;

    // Read Once
    uint32_t core_id, prefix;
    Env::GetCoreId(&core_id, &prefix);

    Dprintf("Current core: %d, %d\n", core_id, prefix);
    uint64_t ret[GP_COUNTER];
    uint64_t ret_end[GP_COUNTER];

    ReadResult rd_ret;
    auto status = pfc.PerCoreRead(true, ret, &rd_ret);
    Dprintf("Number of events: %d\n", rd_ret.num_event);
    for (int i = 0; i < rd_ret.num_event; ++i) {
        Dprintf("Ret: %lld\n", ret[i]);
    }

    // Do job
    jobs.DoJob(testutl::Jobs::MULTIPLY, 1);

    // Read again
    status = pfc.PerCoreRead(false, ret_end, &rd_ret);
    Dprintf("Number of events: %d\n", rd_ret.num_event);
    for (int i = 0; i < rd_ret.num_event; ++i) {
        Dprintf("Ret: %lld\n", ret_end[i]);
    }
    for (int i = 0; i < rd_ret.num_event; ++i) {
        Dprintf("Difference: %lld\n", ret_end[i]-ret[i]);
    }

    Dprintf("Done Test Perfmon collector\n");
}

void TestMtmcProfiler() {
    // Test util setup
    testutl::Jobs jobs;
    // ------- Setup End --------

    mtmc::MTMCProfiler mtmc_prof("/home/intel/work/mtmc/cpp/tests/configs/configExampleCaches.txt");

    mtmc_prof.Init();

    auto params_info = mtmc_prof.GetParamsInfo();

    // First job
    mtmc_prof.LogStart(params_info, "NULL");
    mtmc_prof.DebugPrint();
    jobs.DoJob(testutl::Jobs::MEMCPY, 50);
    mtmc_prof.LogEnd();

    mtmc_prof.DebugPrint();

    // Second job
    mtmc_prof.LogStart(params_info, "NULL");
    jobs.DoJob(testutl::Jobs::MULTIPLY, 50);
    mtmc_prof.LogEnd();

    mtmc_prof.DebugPrint();
}

void TestGuardSampler() {
    GuardSampler gs{};

    printf("Start\n");

    gs.Read("/home/intel/work/mtmc/scripts/perf.data");

    printf("Read\n");

    sleep(1);

    printf("Stop\n");

    gs.Stop();

    sleep(1);
}

void TestMTMCClose(std::string& log_folder) {
    auto prof = new mtmc::MTMCProfiler();
    prof->Init();

    sleep(1);

    prof->Close();
    delete prof;
}

int main(int argc, char** argv) {

    Dprintf("Test done\n");

    return 0;
}