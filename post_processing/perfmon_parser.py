#Copyright 2022 Intel Corporation
#
#Licensed under the Apache License, Version 2.0 (the "License");
#you may not use this file except in compliance with the License.
#You may obtain a copy of the License at
#
#http://www.apache.org/licenses/LICENSE-2.0
#
#Unless required by applicable law or agreed to in writing, software
#distributed under the License is distributed on an "AS IS" BASIS,
#WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#See the License for the specific language governing permissions and
#limitations under the License.


import numpy as np
import os
import json
import parser as ps

class PerfmonParser:
    def __init__(self, tma_cal, config_file):
        # self.metrics, self.event_names = self.ReadPmuConfig(config_file)
        # self.equs = tma_cal.EquLookup(self.metrics, self.event_names)
        self.tma_cal = tma_cal
        self.event_names, self.metrics, self.always_topdown, self.cnsts_list, self.overall_cnsts = self.ReadPmuConfigJson(config_file)
        assert(len(self.event_names) == len(self.metrics))

        self.equs, self.perf_equs, self.cmp_equs, self.cmp_perf_equs, self.sorted_cnsts_alias, self.sorted_cnsts_idx = [], [], [], [], [], []
        for i in range(len(self.metrics)):
            equs, perf_equs, cnsts = tma_cal.EquLookup(self.metrics[i], self.event_names[i])
            self.equs.append(equs)
            self.perf_equs.append(perf_equs)
            self.cmp_equs.append([ps.expr(i).compile() for i in equs])
            self.cmp_perf_equs.append([ps.expr(i).compile() for i in perf_equs])

            sig_cfg_sorted_cnts_alias = []
            sig_cfg_cnts_idx = []
            for j in range(len(cnsts)):
                temp_cnts_equs = []
                temp_cnts_idx = []
                for cnst_name in self.cnsts_list[i]:
                    if cnst_name in cnsts[j].keys():
                        temp_cnts_equs.append(cnsts[j][cnst_name])
                        temp_cnts_idx.append(self.overall_cnsts.index(cnst_name))

                sig_cfg_sorted_cnts_alias.append(temp_cnts_equs)
                sig_cfg_cnts_idx.append(temp_cnts_idx)

            self.sorted_cnsts_alias.append(sig_cfg_sorted_cnts_alias)
            self.sorted_cnsts_idx.append(sig_cfg_cnts_idx)

        if len(self.metrics[0]) != len(self.cmp_equs[0]):   # TODO: HardCODE only support 1 configuration
            print("Warring. Metrics and available equations mismatch, will only display delta PMC value."
                  "For NDA user, please acquire NDA TMA patches for level1, level2, level3 calculation")
            self.equ_valid = False
        else:
            self.equ_valid = True

        print(self.equs)

    def GetMetricPrec(self, metri, i):
        return (metri >> (i * 8)) & 0xff

    def CalTopDown(self, slot_a, metric_a, uop_drop_a, slot_b, metric_b, uop_drop_b):
        return self.tma_cal.CalTopDown(slot_a, metric_a, uop_drop_a, slot_b, metric_b, uop_drop_b)

    def ReadPmuConfig(self, config_file):
        metrics = []
        events = []
        with open(config_file, 'r') as fff:
            lines = fff.readlines()
            States = "FIRST"
            for line in lines:
                if States is "FIRST":
                    if 'Equations' in line:
                        States = "EQU"
                        continue
                    elif 'Group' in line:
                        States = "GRP"
                        continue
                if States == "EQU":
                    if 'End' in line:
                        States = "FIRST"
                        continue
                    metrics.append(line.split("\n")[0])
                if States == "GRP":
                    if 'End' in line:
                        States = "FIRST"
                        continue
                    evt = line.split(",")[0]
                    events.append(evt)
        if len(metrics) == 0 and len(events) == 0:
            print("Warring. Got empty metrics and events from the config file. Config file path or file itself may be invalid. ")
        return metrics, events

    def ReadPmuConfigJson(self, config_json):
        with open(config_json, 'r') as f:
            data = json.load(f)

        always_topdown = False
        events = []
        metrics = []
        cnsts = []

        if "AlwaysSampleTopdown" in data.keys() and data["AlwaysSampleTopdown"] == 1:
            always_topdown = True

        for i in range(len(data['Configs'])):
            events.append(list(data['Configs'][i]['EventList']))
            metrics.append(list(data['Configs'][i]['Metrics']))
            cnsts.append(list(data['Configs'][i]['Constants']))

        if 'OverallCnsts' in data.keys():
            overall_cnsts = data['OverallCnsts']

        return events, metrics, always_topdown, cnsts, overall_cnsts

    def CalPmu(self, pmu_begin, pmu_end, dict_to_append=None, cfg_idx=0, cnsts=[], dur_ms=None):
        output_dict = {}
        if dict_to_append is not None:
            output_dict = dict_to_append

        pmu_begin_n = np.array(pmu_begin).astype(np.double)
        pmu_end_n = np.array(pmu_end).astype(np.double)
        pmu_delta_n = pmu_end_n - pmu_begin_n

        a = pmu_delta_n
        B = pmu_begin
        C = pmu_end
        D = []

        for perf_equ in self.cmp_perf_equs[cfg_idx]:
            D.append(eval(perf_equ))

        if self.equ_valid:
            for idx, key in enumerate(self.metrics[cfg_idx]):
                for i, cnst_alisa in enumerate(self.sorted_cnsts_alias[cfg_idx][idx]):
                    if dur_ms is not None and cnst_alisa == "durationtimeinmilliseconds":
                        vars()[cnst_alisa] = dur_ms
                    else:
                        vars()[cnst_alisa] = cnsts[self.sorted_cnsts_idx[cfg_idx][idx][i]]

                output_dict["Met-."+key] = round(eval(self.cmp_equs[cfg_idx][idx]),2)
        else:
            for idx, key in enumerate(self.event_names[cfg_idx]):
                output_dict["Met-."+key] = pmu_delta_n[idx]

        return output_dict