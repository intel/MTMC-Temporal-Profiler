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

#include "opentele_exporter.h"

namespace trace     = opentelemetry::trace;
namespace nostd     = opentelemetry::nostd;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace resu_sdk  = opentelemetry::sdk::resource;
namespace jaeger    = opentelemetry::exporter::jaeger;
//namespace otlp    = opentelemetry::exporter::otlp;
opentelemetry::exporter::jaeger::JaegerExporterOptions opts;
namespace util = mtmc::util;

OpenteleDaemon::OpenteleDaemon() {};
OpenteleDaemon::~OpenteleDaemon() {};

[[noreturn]] __attribute__((optimize("O3"))) void OpenteleDaemon::Run() {
    // Sanctity checks
    CheckEnv();
    if (setting_.channel_name.empty()) {
        printf("Daemon Exit due to empty channel name\n");
        return;
    }

    // Init channel
    ipc::channel cc(setting_.channel_name.c_str(), ipc::receiver);
    if (!cc.valid()) {
        printf("Daemon Exit due to channel is not valid\n");
        return;
    }
    std::unordered_map<std::string, ipc::shm::handle> shm_map;

    auto jaeger_ip_env = getenv("JAEGER_IP");
    auto jaeger_ip = std::string("localhost");
    if (jaeger_ip_env) {
        jaeger_ip = std::string(jaeger_ip_env);
    }

    opts.endpoint = jaeger_ip;
    auto exporter  = jaeger::JaegerExporterFactory::Create(opts);
//    opentelemetry::sdk::trace::BatchSpanProcessorOptions opt;
//    auto processor = trace_sdk::BatchSpanProcessorFactory::Create(std::move(exporter), opt);
    auto processor = trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
    auto inst_scope = opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create("MTMC-OTLE-Exporter", "v0.2", "");

    auto time_cast = [](uint64_t ns) -> std::chrono::system_clock::time_point{
        auto d = std::chrono::nanoseconds{ns};
        std::chrono::system_clock::time_point tp{std::chrono::duration_cast<std::chrono::system_clock::duration>(d)};
        return tp;
    };

    // Shared between worker and host
    std::queue<ipc::shm::handle> hdlr_queue;
    std::queue<mtmc::ShmIpcLoad> pld_queue;
    std::mutex queue_mux;

    auto export_worker = [&](int idx){
        while (true) {

            // Queue Mutex
            queue_mux.lock();
            if (hdlr_queue.empty()) {
                queue_mux.unlock();
                // Queue Mutex Done
                sleep(1);
                continue;
            }
            ipc::shm::handle shm_hdlr = std::move(hdlr_queue.front());
            mtmc::ShmIpcLoad pld = pld_queue.front();
            hdlr_queue.pop();
            pld_queue.pop();
            queue_mux.unlock();
            // Queue Mutex Done

            // Locate starting of the data
            auto shm_status = (mtmc::ShmIpcStatus *) ((char *) shm_hdlr.get() + pld.data_offset);
            auto mtmc_data = (mtmc::SingleProfile *) ((char *) shm_hdlr.get() + pld.data_offset +
                                                      sizeof(mtmc::ShmIpcStatus));

            std::unordered_map<int64_t, opentelemetry::trace::TraceId> traceid_map;

            int count = 0;
            for (int i = 0; i < pld.num_data; ++i) {
                auto &sig_data = mtmc_data[i];
                auto prefix = std::string(sig_data.prefix);

                // [0] INTEROP or "" [1] Op Name [2] Op Type [3] INTEROP Hash id if [0] == INTEROP else Parent HashID [4] INTEROP inputs
                auto prefix_segs = mtmc::util::StringSplit(prefix, ':');
                auto &hash_id = prefix_segs[3];

                // Create span id.
                std::vector<uint8_t> span_id_hash;
                if (sig_data.hash_id == 0) {
                    span_id_hash = util::GenerateUniqueSpanId();
                } else {
                    span_id_hash = util::GenerateUniqueSpanId(sig_data.hash_id);
                }

                // Create parent span id
                std::vector<uint8_t> parent_span_id_hash;
                parent_span_id_hash = util::GenerateUniqueSpanId(sig_data.parent_info.parent_ctx_hash_id);

                // Create per iteration traceid
                opentelemetry::trace::TraceId trace_id;
                if (pld.trace_hash == 0) { // Unique trace_id for every int_prefix value
                    auto trace_id_find = traceid_map.find(sig_data.int_prefix);
                    if (trace_id_find == traceid_map.end()) {
                        trace_id = opentelemetry::trace::TraceId(util::GenerateUniqueTraceId(sig_data.int_prefix, 0));
                        traceid_map.insert({sig_data.int_prefix, trace_id});
                    } else {
                        trace_id = trace_id_find->second;
                    }
                } else { // This branch will make sure every run uses the same trace_id
                    trace_id = opentelemetry::trace::TraceId(
                            util::GenerateUniqueTraceId(pld.trace_hash, pld.trace_hash));
                }

                // Program Info
                trace_api::SpanContext child_ctx(
                        trace_id,
                        trace_api::SpanId(span_id_hash),
                        trace_api::TraceFlags{trace_api::TraceFlags::kIsSampled},
                        false,
                        trace_api::TraceState::GetDefault());

                auto recordable = processor->MakeRecordable();
                recordable->SetName(sig_data.prefix);
                recordable->SetInstrumentationScope(*inst_scope);
                recordable->SetIdentity(child_ctx, trace_api::SpanId(parent_span_id_hash));
                recordable->SetSpanKind(opentelemetry::trace::SpanKind());

                uint64_t start_ns = sig_data.start_ts;
                auto start_tp = time_cast(start_ns);

                uint64_t end_ns = sig_data.end_ts;
                auto end_tp = time_cast(end_ns);

                recordable->SetStartTime(opentelemetry::common::SystemTimestamp(start_tp));
                recordable->SetDuration(end_tp - start_tp);

                std::vector<std::pair<nostd::string_view, opentelemetry::common::AttributeValue>> vec;
                vec.emplace_back("tid", (int64_t) sig_data.tid);
                vec.emplace_back("pthreadid", (int64_t) sig_data.pthread_id);
                vec.emplace_back("int_prefix", sig_data.int_prefix);

                // Parent info
                std::string parent_info = std::to_string(sig_data.parent_info.parent_tid) + "-*-" +
                                          std::to_string(sig_data.parent_info.parent_pthread_id) + "-*-" +
                                          std::to_string(sig_data.parent_info.task_sched_time);
                vec.emplace_back("parent_tid_pthreadid_sched_time",
                                 parent_info);

                // Begin event info
                std::string start_event_info = std::to_string(sig_data.rd_ret_start.core_id) + "-" +
                                               std::to_string(sig_data.rd_ret_start.prefix) + "-" +
                                               std::to_string(sig_data.rd_ret_start.num_event);
                vec.emplace_back("b_coreid_prefix_num_event",
                                 start_event_info);

                // End event info
                std::string end_event_info = std::to_string(sig_data.rd_ret_end.core_id) + "-" +
                                             std::to_string(sig_data.rd_ret_end.prefix) + "-" +
                                             std::to_string(sig_data.rd_ret_end.num_event);
                vec.emplace_back("e_coreid_prefix_num_event",
                                 end_event_info);

                // Begin events
                std::string begin_events;
                for (int j = 0; j < sig_data.rd_ret_start.num_event; ++j) {
                    begin_events += std::to_string(sig_data.ret_start[j]);
                    if (j < sig_data.rd_ret_start.num_event - 1) begin_events += "-";
                }
                vec.emplace_back("b_events", begin_events);

                // End events
                std::string end_events;
                for (int j = 0; j < sig_data.rd_ret_end.num_event; ++j) {
                    end_events += std::to_string(sig_data.ret_end[j]);
                    if (j < sig_data.rd_ret_end.num_event - 1) end_events += "-";
                }
                vec.emplace_back("e_events", end_events);

                // Config id
                vec.emplace_back("mux_id", sig_data.multiplex_idx);
                vec.emplace_back("cfg_id", pld.configs_id);

                // Constant var
                std::string cnsts;
                for (int j = 0; j < pld.cnsts_length; ++j) {
                    cnsts += std::to_string(pld.cnsts[j]);
                    if (j != pld.cnsts_length - 1)
                        cnsts += "-*-";
                }
                vec.emplace_back("cnsts", cnsts);

                recordable->AddEvent("PeriodInfo", std::chrono::system_clock::now(),
                                     opentelemetry::common::KeyValueIterableView<std::vector<std::pair<nostd::string_view,
                                             opentelemetry::common::AttributeValue>>>(vec));

                processor->OnEnd(std::move(recordable));
                if (i % 10240 == 0) {
                    processor->ForceFlush();
                    Dprintf("Worker %d: %d/%d\n", idx, count++, int(pld.num_data)/10240);
                }
                if (!(jaeger_ip == "localhost" || jaeger_ip == "127.0.0.1"))  {
                    usleep(100);

                }
            }

            processor->ForceFlush();
            std::cout << "Worker " << idx << " Process " << pld.num_data << " logs." << std::endl;
            shm_hdlr.release();
        }
    };
    std::vector<std::thread> export_workers;
    for (int i = 0; i < NUM_EXPORT_WORKER; ++i) {
        export_workers.push_back(std::thread(export_worker, i));
    }

    printf("Launch %d workers for exporting.\n", NUM_EXPORT_WORKER);
    printf("Waiting for requests...\n");
    while(true) {
        auto buffer = cc.recv();

        auto pld = (mtmc::ShmIpcLoad*)buffer.data();

        // Get data shm
        ipc::shm::handle shm_hdlr = ipc::shm::handle(pld->shm_name, pld->shm_size, ipc::shm::open);
        if (!shm_hdlr.valid()) {
            printf(FRED("Shm hdlr from name %s is not valid"), pld->shm_name);
            continue;
        }

        // Send receive shm signal
        auto shm_status = (mtmc::ShmIpcStatus *) ((char *) shm_hdlr.get() + pld->data_offset);
        shm_status->done = 1;

        // Push to worker queue
        {
            std::lock_guard<std::mutex> guard(queue_mux);
            hdlr_queue.push(std::move(shm_hdlr));
            pld_queue.push(*pld);
        }

    }

}

void OpenteleDaemon::CheckEnv() {
    setting_.channel_name = DEFAULT_CHANNEL_NAME; // TODO: Here let's hardcode it. Later may get from environ
}

int main() {

    OpenteleDaemon daemon;
    daemon.Run();

    return 0;
}
