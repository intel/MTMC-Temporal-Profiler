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

#include <cstring>

#include "mtmc_profiler.h"
#include "mtmc_temp_profiler.h"
#include "test_util.h"
#include "guard_sampler.h"

#include "tensorflow/core/public/session.h"
#include "tensorflow/core/framework/variant_encode_decode.h"
#include "tensorflow/core/platform/load_library.h"
#include "tensorflow/core/common_runtime/device_mgr.h"
#include "tensorflow/core/lib/core/threadpool.h"

#include <random>

#undef EBPF_CTX_SC
#ifdef EBPF_CTX_SC
#include "ebpf_sampler.h"
#include "ebpf_common.h"
#endif

using namespace tensorflow;

struct SessConfig {
    string pbtxt_path;
    int inter_th;
    int intra_th;
    int inst; // Instance Number
    int64_t batch_size;
    int64_t itr; // Iterations to run
    int timeline_itr; // Iterations to collect timeline
    std::string output_node;
};

template<DataType DTYPE = DT_FLOAT>
Tensor GenerateRandomTensor(const TensorShape& shape) {
    typedef typename EnumToDataType<DTYPE>::Type T;
    Tensor tensor(DTYPE, shape);
    for (auto i = 0; i < tensor.NumElements(); i++)
        tensor.flat<T>()(i) = 10;
    return tensor;
}

double GenerticTFTest(Session* sess, std::vector<std::vector<std::pair<string, Tensor>>>* inputVec, std::vector<std::vector<Tensor>>* outputVec,
                         const std::string& out_node, int itr, int time_s, int invcache = 0) {
    /// Input
    auto input = *inputVec;

    /// Run opt and metadata
    RunOptions ro_trace = RunOptions();
    ro_trace.set_trace_level(RunOptions_TraceLevel_TraceLevel_MAX);
    std::vector<RunMetadata> all_meta;

    /// output
    std::vector<Tensor> output;

    /// init and warm up
    Status st;
    st = sess->Run(input[0], {out_node}, {}, &output);
    if (!st.ok()) std::cout << st.ToString() << std::endl;
    assert(st.ok());
//    std::cout << output[0].DebugString() << std::endl;

    RunMetadata rm;
    time_t accumu_time = 0;
    int64_t accumu_done = 0;
    int flag = 0;
    auto start_tm = testutl::GetTimeStampUs();
    int PROF_ITR = 1;
    auto count_donw_start = testutl::GetTimeStampUs();
    while (testutl::GetTimeStampUs()-testutl::GetTimeStampUs() < time_s * 1000000) {
        for (int i = 0; i < itr; ++i) {
            if (i == 0) {
                sess->Run(input[i], {out_node}, {}, &output);
            } else if ((i == (int) (itr / 2) || flag > 0) && flag < PROF_ITR) {
                sess->Run(ro_trace, input[i], {out_node}, {}, &output, &rm);
                flag += 1;
            } else if (flag >= PROF_ITR && invcache) continue;
            else sess->Run(input[i], {out_node}, {}, &output);

            outputVec->push_back(output);
            accumu_done += 1;

            if (testutl::GetTimeStampUs()-testutl::GetTimeStampUs() > time_s * 1000000) {
                break;
            }
        }

    }

    auto end_tm = testutl::GetTimeStampUs();
    accumu_time += (end_tm - start_tm);

    printf("Runtime: %ld, QPS: %.3f. Latency(ms): %.5f\n", accumu_time, (double) accumu_done / ((double) accumu_time / 1000000),
           ((double) accumu_time / 1000) / (double) accumu_done);

    std::ofstream ofs("/home/intel/work/rdt/rdt_test/tf/timelines/temp", std::ios::out); // output as a step+id file
    ofs << rm.step_stats().SerializeAsString();
    ofs.close();

    sess->Close();

    return (double) accumu_done / ((double) accumu_time / 1000);
}

GraphDef LoadFromPbtxt(tensorflow::Session* session, const std::string& pb_path) {
    GraphDef my_graph_def;

    // Read pb to graph_def
    Status st_ld = ReadTextProto(Env::Default(), pb_path, &my_graph_def);
    if (!st_ld.ok()) {
        std::cout << st_ld.ToString() << std::endl;
        return GraphDef();
    }

    // add graph to session
    Status st_sess_c = session->Create(my_graph_def);
    if (!st_sess_c.ok()) {
        std::cout << st_sess_c.ToString() << std::endl;
        return GraphDef();
    }

    return my_graph_def;
}

int ProcessOutput(const std::string& log_folder) {

    mtmc::MTMCTemprolProfiler& prof = mtmc::MTMCTemprolProfiler::getInstance();
    prof.Finish(mtmc::util::PathJoin(log_folder, "mtmc_raw_" + std::to_string(reinterpret_cast<int64_t>(&prof))));

    return 1;
}

void TestDlrm(SessConfig cfg, const std::string& log_folder) {

    // All Threadpools sharing one instance of MTMC Temporal Profiler
    mtmc::MTMCTemprolProfiler& prof = mtmc::MTMCTemprolProfiler::getInstance();

    // Disable the profiler globally. Only enable it at the place of interest
    prof.GlobalProfilerDisable();

    /// Create session and load graph
    Session* sess;

    SessionOptions so;
    ConfigProto &config = so.config;
    config.set_intra_op_parallelism_threads(cfg.intra_th);
    config.set_inter_op_parallelism_threads(cfg.inter_th);

    auto status = tensorflow::NewSession(so, &sess);
    if (!status.ok()) std::cout << status.ToString() << std::endl;

    GraphDef gd = LoadFromPbtxt(sess, cfg.pbtxt_path);

    printf(FGRN("Session inited and graph loaded\n"));

    /// Output nodes and Input nodes
    string output_node = "out:0";

    std::cout << "Init variables" << std::endl;
    auto st = sess->Run({},{}, {"init:0"}, {});
    std::cout << st.ToString() << std::endl;

    auto batch_size_tensor = Tensor(DT_INT64, {1});
    auto repeat_size_tensor = Tensor(DT_INT64, {1});
    batch_size_tensor.flat<int64>()(0) = cfg.batch_size;
    repeat_size_tensor.flat<int64>()(0) = cfg.itr;

    /// ======= Run the model next ========

    /// Run multi-threading
    std::vector<std::vector<Tensor>> outputs(cfg.inst);
    std::vector<std::thread> ths(cfg.inst);

    /// Run opt and metadata
    RunOptions ro_trace = RunOptions();
    ro_trace.set_trace_level(RunOptions_TraceLevel_TraceLevel_MAX);
    RunMetadata rm;

    /// Data
    std::vector<std::pair<std::string, Tensor>> data;
    data.push_back({"dense_in:0", GenerateRandomTensor<DT_FLOAT>({cfg.batch_size, 13})});
    data.push_back({"cat_in:0", GenerateRandomTensor<DT_INT64>({cfg.batch_size, 26})});

    printf(FBLU("START THREADS\n"));

    auto worker = [&](int idx) {
        std::vector<Tensor> out;
        Status wst;
        int timeline_cntr = 0;

        prof.GlobalProfilerEnable();

        for (int i = 0; i < cfg.itr; ++i) {

            // Set the int prefix to represent the iteration number
            prof.SetGlobalIntPrefix(i);

            // Print profiler storage usage every 10 iterations
            if (i % 10 == 0) {
                printf(FYEL("Iteration: %d. MTMC-StorageSize uses: %.2f MB\n"), i, (double)prof.StorageSize()/1e6);
            }

            if (idx == 0 && (i == (int)(cfg.itr/2) || (timeline_cntr > 0 && timeline_cntr < cfg.timeline_itr) )) {
                wst = sess->Run(ro_trace, data, {output_node}, {}, &out, &rm);
                timeline_cntr += 1;
            }
            else {
                wst = sess->Run(data,{output_node}, {}, &out);
            }
        }

        prof.GlobalProfilerDisable();

        std::cout << wst.ToString() << std::endl;
    };

    /// WARM UP
    for (int i = 0; i < cfg.inst; ++i) {
        ths[i] = std::thread(worker, -1);
    }
    for (auto& th : ths) th.join();

#ifdef EBPF_CTX_SC
    auto& prof = mtmc::MTMCTemprolProfiler::getInstance();
    auto collector = prof.DebugGetProfilerRef().DebugAcquirePerfmonCollector();
    mtmc::EbpfCtxScConfig ebpf_cfg;
    ebpf_cfg.PerCoreStorageLength = 30000;
    ebpf_cfg.StorageMaxByte = 10e9;
    ebpf_cfg.WeakUpIntvMs = 250;
    auto& ebpf_collector = mtmc::EbpfSampler::GetInstance();
    auto ebpf_status = ebpf_collector.EbpfCtxScInit(ebpf_cfg, mtmc::util::GetCurrAvailableCPUList(), collector->DebugAcquireEbpfCpuFdPairs());
    assert(ebpf_status == 1);
    sleep(5);

    Dprintf(FGRN("Start EBPF Init done\n"));
    ebpf_collector.EbpfCtxScStart();
    sleep(1);
#endif

    /// RUN
    auto time_st = testutl::GetTimeStamp();
    for (int i = 0; i < cfg.inst; ++i) {
        ths[i] = std::thread(worker, i);
    }
    for (auto& th : ths) th.join();

    auto time_end = testutl::GetTimeStamp();
    auto time_elps = double(testutl::GetUsFromTP(time_end) - testutl::GetUsFromTP(time_st))/1000000;
    double qps = (cfg.itr * cfg.batch_size)/time_elps;
    printf(FRED("QPS: %.2f Avg Time: %.2f us Total done: %.0lu, Time: %.2f s\n"), qps, 1/qps, cfg.itr * cfg.batch_size, time_elps);

    // TODO: ABSOLUTE PATH
    std::ofstream ofs(mtmc::util::PathJoin(log_folder,"step_state"), std::ios::out); // output as a step+id file
    ofs << rm.step_stats().SerializeAsString();
    ofs.close();
    rm.step_stats().SerializeAsString();

#ifdef EBPF_CTX_SC
    sleep(1);
    ebpf_collector.EbpfCtxScStop();
    ebpf_collector.EbpfCtxScClose();
    ebpf_collector.DebugPrintEbpfCtxData();
    ebpf_collector.DebugExportEbpfCtxData("/home/intel/work/mtmc/cpp/tests/test_logs/mtmc_tf_tests_logs/ebpf.txt");
#endif

    ProcessOutput(log_folder);

    sess->Close();
    Reset(so, {});
    delete sess;

    sleep(5);
}

double TestBf16MatMul(int input_size, int threadsNum = 14, const string& pbfile = "/home/intel/work/rdt/rdt_test/tf/graph/fp32-bf16cast-matmul-fp32cast.pbtxt") {
    Status status;
    Session *sess;

    SessionOptions so;
    ConfigProto &config = so.config;
    std::cout << threadsNum << std::endl;
    config.set_intra_op_parallelism_threads(threadsNum);
    config.set_inter_op_parallelism_threads(threadsNum);
    config.set_use_per_session_threads(true);

    status = tensorflow::NewSession(so, &sess);
    assert(status.ok());
    LoadFromPbtxt(sess, pbfile);

    /// Input data
    std::vector<std::vector<std::pair<string, Tensor>>> input(input_size);
    auto origA = GenerateRandomTensor<DT_FLOAT>({400, 1208});
    auto origB = GenerateRandomTensor<DT_FLOAT>({1208, 1024});

#pragma omp parallel for // NOLINT(openmp-use-default-none)
    for (int i = 0; i < input_size; ++i) {
        std::vector<std::pair<string, Tensor>> temp;
        temp.emplace_back("in0:0", origA);
        temp.emplace_back("in1:0", origB);
        input[i] = temp;
    }

    std::vector<std::vector<Tensor>> output;

    return GenerticTFTest(sess, &input, &output, "out:0", input_size, 0);
}

int main(int argc, char* argv[]) {

    std::string log_folder = std::string(argv[5]);
    std::string pbtxt_file = std::string(argv[6]);

    std::cout << pbtxt_file << std::endl;

    // Init Guard sampler
#ifdef EBPF_CTX_SC
#else
//    mtmc::GuardSampler gs;
//    gs.Read(mtmc::util::PathJoin(log_folder, "perf.data"));
#endif

    // TODO: MODEL PATH HARDCODE
    SessConfig cfg = {.pbtxt_path = pbtxt_file,
            .inter_th=48,
            .intra_th=48,
            .inst=1,
            .batch_size=256,
            .itr=100,
            .timeline_itr=20,
            .output_node="full"};

    cfg.itr = std::atoi(argv[1]); // Number of iterations
    cfg.timeline_itr = std::atoi(argv[2]); // Number of consecutive iterations that you would like to collect timeline logs
    cfg.inter_th = std::stoi(argv[3]); // Number of inter-op in the thread pool
    cfg.intra_th = std::stoi(argv[4]); // Number of intra-op in the thread pool

    TestDlrm(cfg, log_folder);

#ifdef EBPF_CTX_SC
#else
//    gs.Stop();
//    gs.TimeExport(mtmc::util::PathJoin(log_folder, "timecal.txt"));
#endif
}
