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

#include "mtmc_temp_profiler.h"
#include "mtmc_profiler.h"
#include "test_util.h"

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
            profiler prof;
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
            profiler prof;
            Assert(prof.Init() == -1, "[Init] Wrong config address");
        }
        else if (test_id == 2) {
            // Invalid config file
            /**
             * CT187: Verify that errors and exceptions are detected and handled appropriately
             * T403: Verify that errors and exceptions are securely handled
             * T519: Test that input validation is done on all forms of input
             *
             * testing_config0.txt: Invalid event Hex input.
             * testing_config1.txt: Invalid config syntax. No "End" at the bottom of a config group
             * testing_config2.txt: Invalid config syntax. No "Group,xxx" at the beginning of a config file
             * testing_config3.txt: Invalid number of config. Number of configuration exceed the maximum allowance
             * testing_config4.txt: Invalid event/umask
             * testing_config5.txt: Integer overflow at event/umask
             * testing_config6.txt: Integer overflow at event/umask
             * testing_config7.txt: symbolic link
             */
            for (int i = 0; i < 8; ++i) {
                std::string file_name = "testing_config" + std::to_string(i) + ".txt";
                setenv("MTMC_CONFIG", mtmc::util::PathJoin(config_addr,file_name).c_str(), 1);
                profiler prof;
                Assert(prof.Init() == -1, "[Init] Invalid "+file_name);
            }
        }
        else if (test_id == 3) {
            // Init multiple instance of the profiler
            /**
             * CT187: Verify that errors and exceptions are detected and handled appropriately
             * T403: Verify that errors and exceptions are securely handled
             */
            setenv("MTMC_CONFIG", mtmc::util::PathJoin(config_addr,"functional_config.txt").c_str(),1);
            profiler prof0;
            profiler prof1;
            int ret0 = prof0.Init();
            int ret1 = prof1.Init();
            Assert(ret0 == 1 && ret1 == 1, "[Init] multiple instance init");
            prof0.Close();
            prof1.Close();
        }
            /** Export thread pool information */
        else if (test_id == 4) {
            // Empty, wrong address
            /**
             * CT187: Verify that errors and exceptions are detected and handled appropriately
             * T403: Verify that errors and exceptions are securely handled
             * T519: Test that input validation is done on all forms of input
             */
            profiler prof;
            unsetenv("MTMC_THREAD_EXPORT");
            Assert(prof.ExportThreadPoolInfo({111,222}) == -1, "[ExportThreadPoolInfo] Empty MTMC_THREAD_EXPORT env var");
            setenv("MTMC_THREAD_EXPORT", "ssdjfioan",1);
            Assert(prof.ExportThreadPoolInfo({111,222}) == -1, "[ExportThreadPoolInfo] Wrong MTMC_THREAD_EXPORT env var");
        }
            /** GetParamsInfo() tests */
        else if (test_id == 5) {
            /**
             * CT187: Verify that errors and exceptions are detected and handled appropriately
             * T403: Verify that errors and exceptions are securely handled
             */
            // profiler with cache settings
            setenv("MTMC_THREAD_EXPORT", mtmc::util::PathJoin(config_addr,"functional_config.txt").c_str(),1);
            profiler prof;

            // No init should return empty
            auto parent_info = prof.GetParamsInfo();
            Assert(parent_info.task_sched_time == 0 && parent_info.parent_tid == 0 && parent_info.parent_pthread_id == 0
                    , "No init GetParamsInfo() test");
        }
            /** LogStart() tests */
        else if (test_id == 6) {
            /**
             * CT187: Verify that errors and exceptions are detected and handled appropriately
             * T403: Verify that errors and exceptions are securely handled
             */
            setenv("MTMC_THREAD_EXPORT", mtmc::util::PathJoin(config_addr,"functional_config.txt").c_str(),1);
            profiler prof;

            auto ret = prof.LogStart({},"NULL");
            Assert(ret == -1, "No init LogStart() test");
        }
            /** LogEnd() tests */
        else if (test_id == 7) {
            /**
             * CT187: Verify that errors and exceptions are detected and handled appropriately
             * T403: Verify that errors and exceptions are securely handled
             */
            // LogEnd() without init
            setenv("MTMC_THREAD_EXPORT", mtmc::util::PathJoin(config_addr,"functional_config.txt").c_str(),1);
            profiler prof;
            auto ret = prof.LogEnd();
            Assert(ret == -1, "No init LogEnd() test");

            // LogEnd() before LogStart()
            prof.Init();
            ret = prof.LogEnd();
            Assert(ret == -1, "LogEnd() before LogStart() test");

        }
        else if (test_id == 8) {
            /**
             * CT187: Verify that errors and exceptions are detected and handled appropriately
             * T403: Verify that errors and exceptions are securely handled
             */
            // Finish() without init
            setenv("MTMC_THREAD_EXPORT", mtmc::util::PathJoin(config_addr,"functional_config.txt").c_str(),1);
            profiler prof;

            auto ret = prof.Finish(mtmc::util::PathJoin(log_addr, "test08.txt"));
            Assert(ret == -1, "No init Finish() test");

            // Finish with wrong address test
            prof.Init();
            ret = prof.Finish("/xxxxx/xxxxx/xxxxx");
            Assert(ret == -1, "Wrong address Finish() test");

        }
        else if (test_id == 9) {
            /**
             * CT187: Verify that errors and exceptions are detected and handled appropriately
             * T403: Verify that errors and exceptions are securely handled
             */
            // Init one instance twice
            setenv("MTMC_CONFIG", mtmc::util::PathJoin(config_addr, "functional_config.txt").c_str(),1);
            profiler prof0;
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

    void FunctionalTest() {
        profiler prof;
        setenv("MTMC_THREAD_EXPORT", mtmc::util::PathJoin(config_addr, "functional_config.txt").c_str(),1);
        prof.Init();

        auto parm = prof.GetParamsInfo();
        prof.LogStart(parm, "NULL");
        int ret = 0;
        for (int i = 0; i < 1000; ++i) {
            ret += rand() % 10;
        }
        prof.LogEnd();
//        prof.DebugPrint();
    }

}

int main(int argc, char* argv[]) {
    assert(argc > 2);

    tests::config_addr = std::string(argv[1]);
    tests::log_addr = std::string(argv[2]);

    tests::TestMTMCErrorReportAll();
}