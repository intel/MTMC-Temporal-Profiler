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

namespace mtmc {

    int PerfmonConfig::ReadConfig(const std::string &file_path, std::vector<InputConfig> *config_vec, ProfilerSetting* mtmc_setting) {

        if (config_vec == nullptr || mtmc_setting == nullptr) {
            Dprintf(FRED("Null pointer is passed to the function, read config failed\n"));
            return -1;
        }

        struct InputConfig a = {
                .event_num = 0,
                .names = {"","","","","","","",""},
                .attr_arr = {{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0}},
                .cpu_arr = {0,0,0,0,0,0,0,0,0,0,0,0},
                .pid_arr = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
                .fd = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
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
        }

        std::ifstream cfg_input;
        int fd = open(file_path.c_str(), O_RDWR|O_NOFOLLOW);
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

        /// Debug, print output here:
        for (auto& cfg : *config_vec) {
            Dprintf("Config: %d event(s)\n", cfg.event_num);
            for (int i = 0; i < cfg.event_num; ++i) {
                Dprintf("Event %d, %s, %#010x\n", i, cfg.names[i].c_str(), cfg.attr_arr[i].config);
            }
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
        a.attr_arr[0].exclude_kernel = true;

        a.names[1] = "Perf_Metrics";
        a.attr_arr[1].type = PERF_TYPE_RAW;
        a.attr_arr[1].size = sizeof(struct perf_event_attr);
        a.attr_arr[1].config = 0x8000;
        a.attr_arr[1].exclude_kernel = true;

        a.names[2] = "INT_MISC.UOP_DROPPING";
        a.attr_arr[2].type = PERF_TYPE_RAW;
        a.attr_arr[2].size = sizeof(struct perf_event_attr);
        a.attr_arr[2].config = X86_CONFIG(.event=0xad, .umask=0x10, .inv=0, .cmask=0);
        a.attr_arr[2].exclude_kernel = true;

        a.rd_setting.min_reset_intrvl_ns = reset_intrvl_ns;

        config_vec->push_back(a);

        return 1;
    }

}
