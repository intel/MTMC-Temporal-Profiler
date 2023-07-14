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

#include <cassert>
#include <memory>
#include <vector>
#include <thread>
#include <random>

#include "mtmc_temp_profiler.h"
#include "mtmc_profiler.h"
#include "test_util.h"

#include "exporter.h"
#ifdef OTL_EXPORTER
#endif

#ifdef EBPF_CTX_SC
#include "ebpf_sampler.h"
#include "ebpf_common.h"
#endif

//#define profiler mtmc::MTMCProfiler
#define profiler mtmc::MTMCTemprolProfiler

using namespace testutl;

namespace tests {

    static std::string config_addr;
    static std::string log_addr;

    /**
     * CT187: Verify that errors and exceptions are detected and handled appropriately
     * T403: Verify that errors and exceptions are securely handled
     * T519: Test that input validation is done on all forms of input
     */
    void TestMTMCErrorReport(int test_id) {
        /** Config file + Init() */
        if (test_id == 0) {
            // Empty
            /**
             * CT187: Verify that errors and exceptions are detected and handled appropriately
             * T403: Verify that errors and exceptions are securely handled
             * T519: Test that input validation is done on all forms of input
             */
            setenv("MTMC_CONFIG", "", 1);
            profiler& prof = mtmc::MTMCTemprolProfiler::getInstance();
            Assert(prof.Init() == -1, "[Init] Empty config env");
        }
        else if (test_id == 1) {
            // Wrong address
            /**
             * CT187: Verify that errors and exceptions are detected and handled appropriately
             * T403: Verify that errors and exceptions are securely handled
             * T519: Test that input validation is done on all forms of input
             */
            setenv("MTMC_CONFIG", "asjijfppksa", 1);
            profiler& prof = mtmc::MTMCTemprolProfiler::getInstance();
            Assert(prof.Init() == -1, "[Init] Wrong config address");
        }
        else if (test_id == 2) {
            // Invalid config file
            /**
             * CT187: Verify that errors and exceptions are detected and handled appropriately
             * T403: Verify that errors and exceptions are securely handled
             * T519: Test that input validation is done on all forms of input
             *
             * testing_config0.txt: Invalid SwitchIntvl
             * testing_config1.txt: Invalid Export mode
             * testing_config2.txt: Inconsistent EventList and Events
             * testing_config3.txt: Invalid Configuration JSON
             * testing_config4.txt: Invalid event/umask
             */
            for (int i = 0; i < 5; ++i) {
                std::string file_name = "testing_config" + std::to_string(i) + ".txt";
                setenv("MTMC_CONFIG", mtmc::util::PathJoin(config_addr,file_name).c_str(), 1);
                profiler& prof = mtmc::MTMCTemprolProfiler::getInstance();
                Assert(prof.Init() == -1, "[Init] Invalid "+file_name);
            }
        }
        else if (test_id == 3) {
            // Init multiple instance of the profiler
            /**
             * CT187: Verify that errors and exceptions are detected and handled appropriately
             * T403: Verify that errors and exceptions are securely handled
             */
            setenv("MTMC_CONFIG", mtmc::util::PathJoin(config_addr,"example_cfg_mem.json").c_str(),1);
                printf("str:%s\n", mtmc::util::PathJoin(config_addr, "example_cfg_mem.json").c_str());
            profiler& prof0 = mtmc::MTMCTemprolProfiler::getInstance();
            profiler& prof1 = mtmc::MTMCTemprolProfiler::getInstance();
            int ret0 = prof0.Init();
            int ret1 = prof1.Init();
            Assert(ret0 == 1 && ret1 == 1, "[Init] multiple instance init");
            prof0.Close();
            prof1.Close();
        }
            /** Export thread pool information */
        else if (test_id == 4) {
            /**
             * CT187: Verify that errors and exceptions are detected and handled appropriately
             * T403: Verify that errors and exceptions are securely handled
             * T519: Test that input validation is done on all forms of input
             */
            printf("Skip Test 4.\n");
            profiler& prof0 = mtmc::MTMCTemprolProfiler::getInstance();
            int ret0 = prof0.Init();
            int ret1 = prof0.Init();
            Assert(ret0 == 1 && ret1 == 1, "[Init] Single instance init multiple times");
            prof0.Close();
        }
            /** GetParamsInfo() tests */
        else if (test_id == 5) {
            /**
             * CT187: Verify that errors and exceptions are detected and handled appropriately
             * T403: Verify that errors and exceptions are securely handled
             */
            // profiler with cache settings
            setenv("MTMC_CONFIG", mtmc::util::PathJoin(config_addr,"example_cfg_mem.json").c_str(),1);
            profiler& prof = mtmc::MTMCTemprolProfiler::getInstance();

            // No init should return empty
            auto parent_info = prof.GetParamsInfo();
            Assert(parent_info.task_sched_time == 0 && parent_info.parent_tid == 0 && parent_info.parent_pthread_id == 0
                    , "[Uninitialized] Uninitialized GetParamsInfo() test");
        }
            /** LogStart() tests */
        else if (test_id == 6) {
            /**
             * CT187: Verify that errors and exceptions are detected and handled appropriately
             * T403: Verify that errors and exceptions are securely handled
             */
            setenv("MTMC_CONFIG", mtmc::util::PathJoin(config_addr,"example_cfg_mem.json").c_str(),1);
            profiler& prof = mtmc::MTMCTemprolProfiler::getInstance();

            auto ret = prof.LogStart(mtmc::ParamsInfo{},"NULL");
            Assert(ret == -1, "[Uninitialized] Uninitialized LogStart() test");
        }
            /** LogEnd() tests */
        else if (test_id == 7) {
            /**
             * CT187: Verify that errors and exceptions are detected and handled appropriately
             * T403: Verify that errors and exceptions are securely handled
             */
            // LogEnd() without init
            setenv("MTMC_CONFIG", mtmc::util::PathJoin(config_addr,"example_cfg_mem.json").c_str(),1);
            profiler& prof = mtmc::MTMCTemprolProfiler::getInstance();
            auto ret = prof.LogEnd();
            Assert(ret == -1, "[Uninitialized] Uninitialized LogEnd() test");

            // LogEnd() before LogStart()
            prof.Init();
            ret = prof.LogEnd();
            Assert(ret == -1, "[Data collect] LogEnd() before LogStart() test");
            prof.Close();
        }
        else if (test_id == 8) {
            /**
             * CT187: Verify that errors and exceptions are detected and handled appropriately
             * T403: Verify that errors and exceptions are securely handled
             */
            // Finish() without init
            setenv("MTMC_CONFIG", mtmc::util::PathJoin(config_addr,"example_cfg_mem.json").c_str(),1);
            profiler& prof = mtmc::MTMCTemprolProfiler::getInstance();

            auto ret = prof.Finish(mtmc::util::PathJoin(log_addr, "test08.txt"));
            Assert(ret == -1, "[Uninitialized] Finish() test");

            // Finish with wrong address test
            prof.Init();
            ret = prof.Finish("/xxxxx/xxxxx/xxxxx");
            Assert(ret == -1, "[Data collect] Wrong address Finish() test");
            prof.Close();
        }
        else if (test_id == 9) {
            /**
             * CT187: Verify that errors and exceptions are detected and handled appropriately
             * T403: Verify that errors and exceptions are securely handled
             */
            // Init one instance twice
            setenv("MTMC_CONFIG", mtmc::util::PathJoin(config_addr, "functional_config.txt").c_str(),1);
            profiler& prof0 = mtmc::MTMCTemprolProfiler::getInstance();
            int ret0 = prof0.Init();
            int ret1 = prof0.Init();
            // ret2 should be 2 since even init a MTMCProfiler twice, it only count as 1 live profiler
            int ret2 = prof0.Close();
            // ret 3 should be 1 since the perfmon collector has already been closed.
            int ret3 = prof0.Close();
            Assert(ret0 == 1 && ret1 == 1 && ret2 == 2 && ret3 == 1, "[Init] One instance init multiple times");
        }

    }
    void TestMTMCErrorReportAll() {
        const int TOTAL_CASE = 10;
        for (int i = 0; i < TOTAL_CASE; ++i) {
            TestMTMCErrorReport(i);
        }
    }

    void FunctionalTestWithEbpf() {
        setenv("MTMC_CONFIG", mtmc::util::PathJoin(config_addr, "ebpf_cache_config.txt").c_str(),1);
        mtmc::MTMCTemprolProfiler& prof = mtmc::MTMCTemprolProfiler::getInstance();
        prof.Init();
        auto collector = prof.DebugGetProfilerRef().DebugAcquirePerfmonCollector();

#ifdef EBPF_CTX_SC
        mtmc::EbpfCtxScConfig cfg;
        cfg.PerCoreStorageLength = 30000;
        cfg.StorageMaxByte = 10e9;
        cfg.WeakUpIntvMs = 500;
        auto& ebpf_collector = mtmc::EbpfSampler::GetInstance();
        auto status = ebpf_collector.EbpfCtxScInit(cfg, mtmc::util::GetCurrAvailableCPUList(), collector->DebugAcquireEbpfCpuFdPairs());
        assert(status == 1);

        ebpf_collector.EbpfCtxScStart();
#endif

        auto parm = prof.GetParamsInfo();
        int ret = 0;
        for (int i = 0; i < 60 * 100; ++i) {
            prof.LogStart(parm, "NULL");
            usleep(10000);
            ret += rand() % 10;
            prof.LogEnd();
            Dprintf("Storage size: %.2f\n", float(prof.StorageSize())/1e6);
        }

#ifdef EBPF_CTX_SC
        ebpf_collector.EbpfCtxScStop();
        ebpf_collector.DebugPrintEbpfCtxData();
        ebpf_collector.DebugExportEbpfCtxData("/home/intel/work/mtmc/cpp/tests/test_logs/mtmc_tf_tests_logs/ebpf.txt"); // TODO: HARDCODE
#endif

        auto& shm_exporter = mtmc::ShmExporter::GetExporter();
        prof.DebugGetProfilerRef().Finish(shm_exporter);

        Dprintf(FYEL("Stop done\n"), __func__);
    }

    __attribute__ ((optimize("O0"))) void FunctionalTest() {

        std::cout << mtmc::Env::GetTSCFrequencyHz() << std::endl;

        mtmc::MTMCTemprolProfiler& prof = mtmc::MTMCTemprolProfiler::getInstance();
        prof.Init();

        // Do some work. Eg. memcpy
        // Assume L1 = 48k, L2 = 2048k, L3 = 100,000k
        const int L1_SIZE = 48 * 1e3;
        const int L2_SIZE = 2048 * 1e3;
        const int L3_SIZE = 99840 * 1e3;
        constexpr int FP_ARRAY_SIZE = ( L2_SIZE / sizeof(float) ) / 1;

        auto* buffer0 = aligned_alloc(64, FP_ARRAY_SIZE * sizeof(float));
        auto* buffer1 = aligned_alloc(64, FP_ARRAY_SIZE * sizeof(float));
        memset(buffer0, 0, FP_ARRAY_SIZE * sizeof(float));
        memset(buffer0, 0, FP_ARRAY_SIZE * sizeof(float));

        auto start_ns = mtmc::Env::GetClockTimeNs();
        while (mtmc::Env::GetClockTimeNs() - start_ns < 300 * 1e6) {
            memcpy(buffer1, buffer0, FP_ARRAY_SIZE * sizeof(float));
        }

        /* Single Thread Test
           | -------- Region All ---------- |
           | -- Region 0 -- | -- Region 1 --| */
        prof.LogStart(mtmc::ParamsInfo{}, "Region All");

        prof.LogStart(mtmc::ParamsInfo{}, "Region 0");
        // Do some works for 300ms
        start_ns = mtmc::Env::GetClockTimeNs();
        while (mtmc::Env::GetClockTimeNs() - start_ns < 300 * 1e6) {
            memcpy(buffer1, buffer0, FP_ARRAY_SIZE * sizeof(float));
        }
        prof.LogEnd(); // Region 0

        prof.LogStart(mtmc::ParamsInfo{}, "Region 1");
        // Do some works for 300ms
        start_ns = mtmc::Env::GetClockTimeNs();
        while (mtmc::Env::GetClockTimeNs() - start_ns < 300 * 1e6) {
            memcpy(buffer1, buffer0, FP_ARRAY_SIZE * sizeof(float));
        }
        prof.LogEnd(); // Region 1

        prof.LogEnd(); // Region all
        /* Single Thread End */

        /* Multi-Thread Test */
        std::vector<std::thread> ths(10);

        auto worker = [](int i) {
            auto& prof = mtmc::MTMCTemprolProfiler::getInstance();
            prof.LogStart(mtmc::ParamsInfo{}, "Thread " + std::to_string(i)); // Thread i

            // Do some works for 300ms
            auto start_ns = mtmc::Env::GetClockTimeNs();
            auto* buffer_thread = aligned_alloc(64, FP_ARRAY_SIZE * sizeof(int));
            while (mtmc::Env::GetClockTimeNs() - start_ns < 300 * 1e6) {
                int val = rand();
                memset(buffer_thread, val, FP_ARRAY_SIZE * sizeof(int));
            }
            free(buffer_thread);

            prof.LogEnd();  // Thread i end
        };

        for (int i = 0; i < 10; ++i) {
            ths[i] = std::thread(worker, i);
        }
        for (auto& th : ths) th.join();
        /* Multi Thread End */

        for (int i = 0; i < 60; ++i) {
            prof.LogStart(mtmc::ParamsInfo{}, "Region " + std::to_string(i%10));
            start_ns = mtmc::Env::GetClockTimeNs();
            while (mtmc::Env::GetClockTimeNs() - start_ns < 30 * 1e6) {
                memcpy(buffer1, buffer0, FP_ARRAY_SIZE * sizeof(float));
            }
            prof.LogEnd(); // Region 1
        }

        // ---------------------- Export and clean --------------------------

        free(buffer0);
        free(buffer1);

    };

    __attribute__ ((optimize("O0"))) void TestMTMCIndexVec() {
        mtmc::util::IndexVector<int> a(5);

        for (int i = 0; i < 10; ++i) {
            a.PushBack(i);
        }

        a.AsyncClear();

        for (int i = 0; i < 10; ++i) {
            a.PushBack(i);
        }

        a.AsyncClearAndReleaseMemory();

        for (int i = 0; i < 10; ++i) {
            a.PushBack(i);
        }

        printf("Size %ld\n", a.Size());

        return;
    }

}

int main(int argc, char* argv[]) {
//    assert(argc > 2);

//    tests::config_addr = std::string(argv[1]);
//    tests::log_addr = std::string(argv[2]);
//
//    tests::TestMTMCErrorReportAll();

    tests::TestMTMCIndexVec();

//    tests::FunctionalTest();
}