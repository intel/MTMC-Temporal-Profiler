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

#ifndef MTMC_OPENTELE_EXPORTER_H
#define MTMC_OPENTELE_EXPORTER_H

#include <string>
#include <unordered_map>
#include "libipc/ipc.h"
#include "../exporter.h"
#include <queue>

#include "opentelemetry/context/runtime_context.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/exporters/jaeger/jaeger_exporter_factory.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/provider.h"
#define NUM_EXPORT_WORKER 1

struct DaemonSetting {
    std::string channel_name = DEFAULT_CHANNEL_NAME;
};

class Daemon {
public:
    Daemon() = default;
    virtual ~Daemon() = default;

    virtual void CheckEnv() = 0;

    virtual void Run() = 0;

};

class OpenteleDaemon : public Daemon {
public:
    OpenteleDaemon();
    ~OpenteleDaemon() override;

    void CheckEnv() override;

    [[noreturn]] void Run() override;

private:

    DaemonSetting setting_;

};


#endif //MTMC_OPENTELE_EXPORTER_H
