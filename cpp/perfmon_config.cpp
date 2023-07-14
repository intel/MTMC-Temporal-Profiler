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

#include "perfmon_config.h"

using json = nlohmann::json;

namespace mtmc {

    uint64_t X86Config(int event, int umask, int inv, int cmask, int edge) {
        x86_pmu_config conf{};
        conf.bits.event = event;
        conf.bits.umask = umask;
        conf.bits.inv = inv;
        conf.bits.cmask = cmask;
        conf.bits.edge = edge;
        return conf.value;
    };

    int PerfmonConfig::ReadConfig(const std::string &file_path, std::vector<InputConfig> *config_vec, ProfilerSetting* mtmc_setting) {

        if (config_vec == nullptr || mtmc_setting == nullptr) {
            Dprintf(FRED("Null pointer is passed to the function, read config failed\n"));
            return -1;
        }

        mtmc_setting->cfg_type = util::TXT;

        struct InputConfig a = {
                .event_num = 0,
                .names = {"","","","","","","",""},
                .attr_arr = {{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0}},
                .cpu_arr = {0,0,0,0,0,0,0,0,0,0,0,0},
                .pid_arr = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
                .fd = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
                .group_name = "GroupDefaultName",
                .rd_setting = {
                    .min_reset_intrvl_ns = -1,
                    .add_offset = true,
                    .sign_ext = true
                }
        };

        // Reset the event attrs
        for (int i = 0; i < GP_COUNTER; ++i) {
            memset(&(a.attr_arr[i]), 0, sizeof(perf_event_attr));
            a.attr_arr[i].type = PERF_TYPE_RAW;
            a.attr_arr[i].size = sizeof(perf_event_attr);
            a.attr_arr[i].exclude_kernel = 1;
            if (i == 0) {
                a.attr_arr[i].pinned = 1;
            }
        }

        std::ifstream cfg_input;
        int fd = open(file_path.c_str(), O_RDONLY|O_NOFOLLOW);
        if (fd < 0) {
            Dprintf(FRED("Invalid config file. File is not exist or it is a symbolic link\n"));
            return -1;
        }

        __gnu_cxx::stdio_filebuf<char> file_buf(fd, std::ios::in);
        cfg_input.rdbuf()->swap(file_buf);

        /**
         * Config file should be like:
         * Group,XXX
         * EventName,event,umask,inv,cmask\n
         * EventName,event,umask,inv,cmask\n
         * End
         * Group,XXX
         * EventName,event,umask,inv,cmask\n
         * EventName,event,umask,inv,cmask\n
         * End
         * EndConfig
        */
        enum RD_STATE {
            INIT,
            EVENTS
        };
        RD_STATE rd_state = INIT;

        DDprintf("Read\n");
        std::string line;
        InputConfig temp = a;
        while(!cfg_input.eof()) {
            std::getline(cfg_input,line);
            DDprintf("Orig line: %s\n", line.c_str());

            if (line.find("EndConfig") != std::string::npos)
                break;

            if (line.find("End") != std::string::npos) {
                config_vec->push_back(temp);
                temp = a;
                rd_state = INIT;
                continue;
            }

            if (line.find("UseTopdownMetric") != std::string::npos) {
                mtmc_setting->perf_collect_topdown = true;
                continue;
            }

//            line.pop_back();

            std::stringstream ss(line);
            std::string sub_str;

            switch (rd_state) {
                case INIT:
                    /// Group:XXX
                    if (ss.good()) {
                        std::getline(ss, sub_str, ',');
                        if (sub_str == "Group" && ss.good()) {
                            std::getline(ss, sub_str, ',');
                            temp.group_name = sub_str;
                            rd_state = EVENTS;
                            DDprintf("Group,%s\n", sub_str.c_str());
                        } else goto INVALID;
                    } else {
                        INVALID:
                            Dprintf("Config file failed due to did not detect \"Group\" key word or \"Group\" is"
                                    "followed by an empty line\n");
                            return -1;
                    }
                    break;
                case EVENTS:
                    if (temp.event_num >= GP_COUNTER) {
                        Dprintf("Number of event in a group can not exceed %d\n", GP_COUNTER);
                        return -1;
                    }
                    /// EventName,event,umask,inv,cmask\n
                    uint64_t es[4]; // event settings: event umask inv cmask
                    for (int i = 0; i < 5; ++i) {
                        if (ss.good()) {
                            std::getline(ss, sub_str, ',');
                            DDprintf("Sub: %s, %lu\n", sub_str.c_str(),sub_str.size());
                            if (i == 0) {
                                temp.names[temp.event_num] = sub_str;
                            } else {
                                try {
                                    int64_t temp_val = std::stol(sub_str, nullptr, 16);
                                    if (temp_val < 0) throw std::out_of_range("Negative");
                                    es[i - 1] = (uint64_t)temp_val;
                                }
                                catch (std::invalid_argument& ia) {
                                    Dprintf(FRED("Invalid base 16 event configuration: %s\n"), sub_str.c_str());
                                    return -1;
                                }
                                catch (std::out_of_range& io) {
                                    Dprintf(FRED("Event configuration out of range: %s\n"), sub_str.c_str());
                                    return -1;
                                }
                                catch (std::exception& e) {
                                    Dprintf("Event or umask invalid: %s\n", sub_str.c_str());
                                    return -1;
                                }
                            }
                        } else {
                            Dprintf("Config file failed at events state\n");
                            return -1;
                        }
                    }
                    DDprintf("Read: %s,%lu,%lu,%lu,%lu\n", temp.names[temp.event_num].c_str(), es[0],
                              es[1], es[2], es[3]);
                    temp.attr_arr[temp.event_num].config = X86_CONFIG(.event=es[0], .umask=es[1], .inv=es[2],
                                                                      .cmask=es[3]);
                    temp.event_num += 1;
                    break;
            }
            DDprintf("Done Read\n");
        }

        if (config_vec->empty()) {
            Dprintf(FRED("Config reader gets empty configuration\n"));
            return -1;
        }

        close(fd);
        return 1;
    }

    int PerfmonConfig::PerfMetricConfig(std::vector<InputConfig> *config_vec, int32_t reset_intrvl_ns) {

        struct InputConfig a = {
                .event_num = 0,
                .names = {"","","","","","","",""},
                .attr_arr = {{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0}},
                .cpu_arr = {0,0,0,0,0,0,0,0,0,0,0,0},
                .pid_arr = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
                .fd = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
                .group_name = "DefaultGroupName",
                .rd_setting = {
                        .min_reset_intrvl_ns = -1,
                        .add_offset = false,
                        .sign_ext = false
                }
        };

        a.event_num = 3;
        a.names[0] = "Perf_Slots";
        a.attr_arr[0].type = PERF_TYPE_RAW;
        a.attr_arr[0].size = sizeof(struct perf_event_attr);
        a.attr_arr[0].config = 0x400;
        a.attr_arr[0].exclude_kernel = 1;
        a.attr_arr[0].pinned = 1;

        a.names[1] = "Perf_Metrics";
        a.attr_arr[1].type = PERF_TYPE_RAW;
        a.attr_arr[1].size = sizeof(struct perf_event_attr);
        a.attr_arr[1].config = 0x8000;
        a.attr_arr[1].exclude_kernel = 1;
        a.attr_arr[1].pinned = 0;

        a.names[2] = "INT_MISC.UOP_DROPPING";
        a.attr_arr[2].type = PERF_TYPE_RAW;
        a.attr_arr[2].size = sizeof(struct perf_event_attr);
        a.attr_arr[2].config = X86_CONFIG(.event=0xad, .umask=0x10, .inv=0, .cmask=0);
        a.attr_arr[2].exclude_kernel = 1;
        a.attr_arr[2].pinned = 0;

        a.rd_setting.min_reset_intrvl_ns = reset_intrvl_ns;

        config_vec->push_back(a);

//        for (auto& cfg : *config_vec) {
//            cfg.attr_arr[0].inherit = 1;
//        }

//        for (auto& cfg : *config_vec) {
//            for (int j = 0; j < cfg.event_num; ++j) {
//                cfg.attr_arr[j].inherit = 1;
//                cfg.pid_arr[j] = 0;
//                cfg.cpu_arr[j] = -1;
//            }
//        }

        return 1;
    }

    int PerfmonConfig::ReadConfigJson(const std::string &file_path, std::vector<InputConfig> *config_vec, ProfilerSetting *mtmc_setting) {

        if (config_vec == nullptr || mtmc_setting == nullptr) {
            Dprintf(FRED("Null pointer is passed to the function, read config failed\n"));
            return -1;
        }

        mtmc_setting->cfg_type = util::JSON;

        struct InputConfig a = {
                .event_num = 0,
                .names = {"","","","","","","",""},
                .attr_arr = {{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0}},
                .cpu_arr = {0,0,0,0,0,0,0,0,0,0,0,0},
                .pid_arr = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
                .fd = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
                .group_name = "GroupDefaultName",
                .rd_setting = {
                        .min_reset_intrvl_ns = 1, // TODO Reset intv hdcd
                        .add_offset = false,
                        .sign_ext = false
                }
        };

        // Reset the event attrs
        for (int i = 0; i < GP_COUNTER; ++i) {
            memset(&(a.attr_arr[i]), 0, sizeof(perf_event_attr));
            a.attr_arr[i].type = PERF_TYPE_RAW;
            a.attr_arr[i].size = sizeof(perf_event_attr);
            a.attr_arr[i].exclude_kernel = 1;
            if (i == 0) {
                a.attr_arr[i].pinned = 1;
            }
        }

        std::ifstream in(file_path);
        json j;
        in >> j;

        /*
         *  {
         *      "Configs": [
         *          {
         *              "Events": {
         *                  "CPU_CLOCK_THREADS": "00,01,02,03,04", (event,umask,cmask,inv,edge)
         *                  ...
         *              },
         *              "Metrics": [str0, str1, str2...]
         *          },
         *          {
         *              ...
         *          },
         *      ],
         *      "SwitchIntvl": "30s"/"20ms"/"10ns",
         *      "ExportMode": 0/1/2/3
         *  }
         */

        try {
            std::vector<std::map<std::string, std::string>> all_configs;
            std::vector<std::vector<std::string>> all_metrics;
            std::string switch_intvl;

            if (!j.contains("Configs"))
                throw std::runtime_error("No \"Configs\" key works in the MTMC config json");

            /* ------------- General Settings -------------- */
            // Multiplx Interval
            if (!j.contains("SwitchIntvl")) {
                switch_intvl = "200ms";
            }
            else {
                switch_intvl = j["SwitchIntvl"];
            }

            // Export Mode:
            if (j.contains("ExportMode")) {
                mtmc_setting->export_mode = j["ExportMode"];
                if (!(mtmc_setting->export_mode >= 0 && mtmc_setting->export_mode <=3)) {
                    throw std::runtime_error("Invalid export mode. Export mode should be 0,1,2 or 3");
                }
            }
            else {
                mtmc_setting->export_mode = 0;
            }

            // Trace Hash:
            if (j.contains("TraceHash")) {
                mtmc_setting->trace_hash = j["TraceHash"];
            }
            else {
                mtmc_setting->trace_hash = 0;
            }

            // Configs id:
            if (j.contains("ConfigsId")) {
                mtmc_setting->configs_id = j["ConfigsId"];
            }
            else {
                mtmc_setting->configs_id = -1;
            }

            // OverallCnsts:
            if (j.contains("OverallCnsts")) {
                for (auto& elem : j["OverallCnsts"].items()) {
                    mtmc_setting->cnst_var.insert(elem.value().get<std::string>());
                }
            }
            /* ------------- General Settings Ends -------------- */

            /* ------------- Perfmon Event Settings ---------------- */
            int cntr = 0;
            for (auto& elem : j["Configs"]) {
                printf("== Config %d ==\n", cntr);
                all_configs.emplace_back();
                all_metrics.emplace_back();
                auto temp_cfg = a;
                temp_cfg.group_name = std::to_string(cntr);
                temp_cfg.multiplex_intv = util::ConvertTimeToNanoSeconds(switch_intvl);

                int i = 0;

                for (auto& evt : elem["EventList"].items()) {
                    const auto& evt_name = evt.value().get<std::string>();
                    const auto& evt_cfg = elem["Events"][evt_name];

                    printf("Event: %s\n", evt_name.c_str());

                    temp_cfg.names[i] = std::string(evt_name);
                    auto hw_evt_cfg = util::HexToVec(evt_cfg.get<std::string>());
                    if (hw_evt_cfg.size() != 5)
                        throw std::runtime_error("Error. Event " + evt_name + " do not have proper counter info.\n");
                    temp_cfg.attr_arr[i].config = X86Config(hw_evt_cfg[0], hw_evt_cfg[1], hw_evt_cfg[2], hw_evt_cfg[3], hw_evt_cfg[4]);
                    temp_cfg.names[i] = evt_name;
                    all_configs.back().insert({std::string(evt_name), std::string(evt_cfg.get<std::string>())});
                    temp_cfg.event_num++;
                    ++i;
                };

                elem["EventList"].clear();
                for (int ev_num = 0; ev_num < temp_cfg.event_num; ++ev_num) {
                    elem["EventList"].push_back(temp_cfg.names[ev_num]);
                }

                config_vec->push_back(temp_cfg);
                cntr++;
            }
        }
        catch(const std::exception& e) {
            printf(FRED("%s\n"), e.what());
            return -1;
        };

        in.close();

        std::ofstream out(file_path, std::ofstream::out);
        out << j.dump(4);

        return 1;
    }

}
