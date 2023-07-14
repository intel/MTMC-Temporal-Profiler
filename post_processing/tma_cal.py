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
import copy
import os
import json

def MAX(a, b):
    return max(a, b)

def MIN(a, b):
    return min(a, b)

class TmaCal(object):
    def __init__(self):
        self.equation_dict = {}
        return

    def EquLookup(self, metrics, event_names):
        return

    def CalLevel3(self, idx):
        return

def ReadEquDict(path_to_json):
    if not os.path.exists(path_to_json) or not os.path.isfile(path_to_json):
        raise FileNotFoundError("Invalid path to the Metirc Equations")

    with open(path_to_json, 'r') as f:
        equ_json = json.load(f)
        ret_dict = {}

        for metric in equ_json["Metrics"]:
            ret_dict[metric["LegacyName"]] = ([evt['Name'] for evt in metric["Events"]],
                                              metric["Formula"],
                                              {cnst['Name'] : cnst["Alias"] for cnst in metric["Constants"]},
                                              [evt['Alias'] for evt in metric["Events"]])

        return ret_dict

class SprTmaCal(TmaCal):
    def __init__(self, base_frequency=2.7):
        super().__init__()
        self.equation_dict = {
            "TMA_3..L1_Bound(%)": (["EXE_ACTIVITY.BOUND_ON_LOADS", "MEMORY_ACTIVITY.STALLS_L1D_MISS", "CPU_CLK_UNHALTED.THREAD"]
                                   ,"100*(MAX(((a-b)/(c)),(0)))"),
            "TMA_3..L2_Bound(%)": (["MEMORY_ACTIVITY.STALLS_L1D_MISS", "MEMORY_ACTIVITY.STALLS_L2_MISS", "CPU_CLK_UNHALTED.THREAD"]
                                   ,"100*((a-b)/(c))"),
            "TMA_3..L3_Bound(%)": (["MEMORY_ACTIVITY.STALLS_L2_MISS", "MEMORY_ACTIVITY.STALLS_L3_MISS", "CPU_CLK_UNHALTED.THREAD"]
                                   ,"100*((a-b)/(c))"),
            "TMA_3..DRAM_Bound(%)": (["MEMORY_ACTIVITY.STALLS_L3_MISS", "CPU_CLK_UNHALTED.THREAD"]
                                     ,"100*(MIN((((a/(b)))),((1))))"),
            "TMA_3..Store_Bound(%)": (["EXE_ACTIVITY.BOUND_ON_STORES", "CPU_CLK_UNHALTED.THREAD"]
                                      ,"100*(a/(b))"),
            "CPI" : (["CPU_CLK_UNHALTED.THREAD", "INST_RETIRED.ANY"], "a/b"),
            "Freq(GHz)":(["CPU_CLK_UNHALTED.THREAD","CPU_CLK_UNHALTED.REF_TSC"],f'(a/b)*{base_frequency}')
        }

        if os.getenv("PERFMON_GIT_PATH") is None:
            raise Exception("Need to have environ PERFMON_GIT_PATH to intel/perfmon git repo")

        #TODO: HARDCODE
        # self.equation_dict = ReadEquDict(os.path.join("/home/intel/work/knowledge/playground/json_play_ground/perfmon/", "SPR/metrics/sapphirerapids_metrics.json"))
        self.equation_dict = ReadEquDict(os.path.join(os.getenv("PERFMON_GIT_PATH"), "SPR/metrics/sapphirerapids_metrics_pruned.json"))
        self.equation_dict["CPI"] = (["CPU_CLK_UNHALTED.THREAD", "INST_RETIRED.ANY"], "a/b", {}, [])
        self.equation_dict["Freq(GHz)"] = (["CPU_CLK_UNHALTED.THREAD","CPU_CLK_UNHALTED.REF_TSC"],f'(a/b)*{base_frequency}',{},[])

        self.event_replace = {
            "CPU_CLK_UNHALTED.THREAD": "CPU_CLK_UNHALTED.THREAD_P" # Current kernel seems put this onto a gp counter. So use THREAD_P instead of THREAD to make sure kernel can program the right counter
            }

        self.metrics_name_dict = {
            "PERF_METRICS.RETIRING": 0,
            "PERF_METRICS.FRONTEND_BOUND": 2,
            "PERF_METRICS.BAD_SPECULATION": 1,
            "PERF_METRICS.BACKEND_BOUND": 3,
            "PERF_METRICS.FETCH_LATENCY": 6,
            "PERF_METRICS.BRANCH_MISPREDICTS": 5,
            "PERF_METRICS.MEMORY_BOUND": 7,
            "PERF_METRICS.HEAVY_OPERATIONS": 4}

        return

    def PerfMetricEquLookup(self, perf_metrics_name, perf_metric_a, perf_metric_b, slots_a, slots_b):
        def GetMetricPrec(metri, i):
            return (int(metri) >> (i * 8)) & 0xff

        if perf_metrics_name not in self.metrics_name_dict.keys():
            print(f"ERROR: perf_metrics_name {perf_metrics_name} is not in metrics name dict")
            return None

        perf_metrics_id = self.metrics_name_dict[perf_metrics_name]

        ret = float(GetMetricPrec(perf_metric_b, perf_metrics_id) * slots_b - GetMetricPrec(perf_metric_a, perf_metrics_id) * slots_a) / (slots_b - slots_a + 0.00001)

        # if perf_metrics_id == 0:
        #     # print(perf_metrics_id, perf_metric_a, perf_metric_b, slots_a, slots_b)
        #     print(perf_metrics_id, int(perf_metric_a), int(perf_metric_b), GetMetricPrec(perf_metric_b, perf_metrics_id), GetMetricPrec(perf_metric_a, perf_metrics_id))
        #     pass

        return ret

    def EquLookup(self, metrics, event_names):

        def find_occ(char, string):
            return ([n for n in range(len(string)) if string.find(char, n) == n])

        def find_strict_occ(char, string):
            pos = []
            for i in range(len(string)):
                if string[i] == char:
                    if (i == 0 and i+1 < len(string) and string[i+1] not in alphabet) or\
                            (i == len(string) - 1 and string[i-1] not in alphabet) or \
                            (string[i-1] not in alphabet and string[i+1] not in alphabet):
                        pos.append(i)
            return pos

        def replace_occ(rep_pairs, string):
            string_arr = list(string)
            for pair in rep_pairs:
                for idx in pair[0]:
                    string_arr[idx] = pair[1]

            return "".join(string_arr)

        ret = []
        perf_met_dict = {}
        ret_perf = []
        ret_cnst = []
        init = 'a'
        alphabet = [chr(ord(init) + i) for i in range(0, 26)]

        for met in metrics:
            if met in self.equation_dict.keys():
                target_events, equ, cnst_dict, equ_alphabet = self.equation_dict[met]
                to_replace = []
                for idx, evt in enumerate(target_events):

                    if evt in self.event_replace.keys():
                        evt = self.event_replace[evt]

                    if 'PERF_METRICS.' in evt and 'PERF_METRICS' in event_names:
                        perf_mat_idx = event_names.index('PERF_METRICS')
                        perf_slots_idx = event_names.index('TOPDOWN.SLOTS:perf_metrics')

                        if evt in perf_met_dict.keys():
                            to_replace.append((find_strict_occ(equ_alphabet[idx], equ), f"D[{perf_met_dict[evt]}]"))
                        else:
                            perf_met_dict[evt] = len(ret_perf)
                            ret_perf.append(f"self.tma_cal.PerfMetricEquLookup('{evt}', B[{perf_mat_idx}], C[{perf_mat_idx}], B[{perf_slots_idx}], C[{perf_slots_idx}])")
                            to_replace.append((find_strict_occ(equ_alphabet[idx], equ), f"D[{perf_met_dict[evt]}]"))

                    elif evt in event_names:
                        # abc = alphabet[event_names.index(evt)]
                        # print(f"Find {alphabet[idx]} in {equ}")
                        abc = f"a[{event_names.index(evt)}]"
                        # to_replace.append((find_occ(alphabet[idx], equ),abc))
                        to_replace.append((find_strict_occ(equ_alphabet[idx], equ),abc))
                    else:
                        equ = "0"

                ret_cnst.append(cnst_dict)

                if equ != "0":
                    modified_equ = replace_occ(to_replace, equ)
                else:
                    modified_equ = equ
                ret.append(modified_equ)
            else:
                ret.append("0")

        return ret, ret_perf, ret_cnst

    def GenTopdownEqus(self, perf_metrics_rep: str, sub_perf_metrics: str):
        topdown_equ = lambda rep, i : f"(int({rep}) >> int({i} * 8) & 0xff)"

        return topdown_equ(perf_metrics_rep, self.metrics_name_dict[sub_perf_metrics])



    def CalTopDown(self, slot_a, metric_a, uop_drop_a, slot_b, metric_b, uop_drop_b):

        # ret = self.CalTopDownWithoutDiff(slot_a, metric_a, uop_drop_a, slot_b, metric_b, uop_drop_b)
        ret = self.CalTopDownByDiff(slot_a, metric_a, uop_drop_a, slot_b, metric_b, uop_drop_b)

        return ret[0], ret[1]

    def CalTopDownByDiff(self, slot_a, metric_a, uop_drop_a, slot_b, metric_b, uop_drop_b):

        def GetMetricPrec(metri, i):
            return (int(metri) >> (i * 8)) & 0xff

        # print(f"TopDown: {slot_a},{metric_a},{uop_drop_a},{slot_b},{metric_b},{uop_drop_b}")
        slot_delta = slot_b - slot_a + 0.00001
        uop_drop_delta = uop_drop_b - uop_drop_a
        tmam_names = ['TMA_1_..Retiring(%)', 'TMA_1_..Bad Speculation(%)', 'TMA_1_..Front-End bound(%)',
                      'TMA_1_..Back-End Bound(%)',
                      'TMA_2_..Heavy ops(%)', 'TMA_2_..Branch Misprediction(%)', 'TMA_2_..Fetch Lat(%)',
                      'TMA_2_..Mem Bound(%)',
                      'TMA_2_..Light ops(%)', 'TMA_2_..Machine Clear(%)', 'TMA_2_..Fetch BW(%)',
                      'TMA_2_..Core Bound(%)', 'SlotsA,B,Diff', 'Scaled:', 'Output_new:', 'uops']
        temp_tmam = []
        output_tmam = []
        # 0 Retiring
        temp_tmam.append(
            (GetMetricPrec(metric_b, 0) * slot_b - GetMetricPrec(metric_a, 0) * slot_a) / slot_delta)
        # 1 bad speculation
        temp_tmam.append(
            (GetMetricPrec(metric_b, 1) * slot_b - GetMetricPrec(metric_a, 1) * slot_a) / slot_delta)
        # 2 Front-End
        temp_tmam.append(
            (GetMetricPrec(metric_b, 2) * slot_b - GetMetricPrec(metric_a, 2) * slot_a) / slot_delta)
        # 3 Back-End
        temp_tmam.append(
            (GetMetricPrec(metric_b, 3) * slot_b - GetMetricPrec(metric_a, 3) * slot_a) / slot_delta)

        # 4 Heavy Ops
        temp_tmam.append(
            (GetMetricPrec(metric_b, 4) * slot_b - GetMetricPrec(metric_a, 4) * slot_a) / slot_delta)
        # 5 Branch Mispred
        temp_tmam.append(
            (GetMetricPrec(metric_b, 5) * slot_b - GetMetricPrec(metric_a, 5) * slot_a) / slot_delta)
        # 6 Fetch Latency
        temp_tmam.append(
            (GetMetricPrec(metric_b, 6) * slot_b - GetMetricPrec(metric_a, 6) * slot_a) / slot_delta)
        # 7 Memory Bound
        temp_tmam.append(
            (GetMetricPrec(metric_b, 7) * slot_b - GetMetricPrec(metric_a, 7) * slot_a) / slot_delta)

        sum_l1 = temp_tmam[0] + temp_tmam[1] + temp_tmam[2] + temp_tmam[3] + 0.00001
        # % 0 Retiring
        output_tmam.append(round(100 * (temp_tmam[0] / sum_l1), ndigits=3))
        # % 1 Bad Spec
        # output_tmam.append(100 * max((1 - ((temp_tmam[2] / (sum_l1) - uop_drop_delta / slot_delta))
        #                        + (temp_tmam[3] / (sum_l1))
        #                        + (temp_tmam[0] / (sum_l1))),0))
        output_tmam.append(round(100 * (
                1 - (temp_tmam[0] / sum_l1) - (temp_tmam[2] / sum_l1 - uop_drop_delta / slot_delta) - (
                temp_tmam[3] / sum_l1)), ndigits=3))
        # % 2 Front-End
        output_tmam.append(round(100 * (temp_tmam[2] / sum_l1 - uop_drop_delta / slot_delta), ndigits=3))
        # % 3 Back-End
        output_tmam.append(round(100 * (temp_tmam[3] / sum_l1), ndigits=3))
        # % 4 Heavy Ops
        output_tmam.append(round(100 * (temp_tmam[4] / sum_l1), ndigits=3))
        # % 5 Branch Mispred
        output_tmam.append(round(100 * (temp_tmam[5] / sum_l1), ndigits=3))
        # % 6 Fetch Latency
        output_tmam.append(round(100 * (temp_tmam[6] / sum_l1 - uop_drop_delta / slot_delta), ndigits=3))
        # % 7 Memory Bound
        output_tmam.append(round(100 * (temp_tmam[7] / sum_l1), ndigits=3))
        # % 8 Light Ops
        output_tmam.append(round(output_tmam[0] - output_tmam[4], ndigits=3))
        # % 9 Machine Clear
        output_tmam.append(round(output_tmam[1] - output_tmam[5], ndigits=3))
        # % 10 Fetch Bandwidth
        output_tmam.append(round(output_tmam[2] - output_tmam[6], ndigits=3))
        # % 11 Core Bound
        output_tmam.append(round(output_tmam[3] - output_tmam[7], ndigits=3))

        # for tmem in output_tmam:
        #     # if tmem > 100 or tmem < -50:
        #     if True:
        #         print(output_tmam)
        #         print(f"uops-drops: {uop_drop_a}-{uop_drop_b}-{uop_drop_delta}")
        #         print(f"Slots: {slot_a}-{slot_b}-{slot_delta}")
        #         print(f"{GetMetricPrec(metric_a, 0)}-{GetMetricPrec(metric_a, 1)}-{GetMetricPrec(metric_a, 2)}-{GetMetricPrec(metric_a, 3)}")
        #         print(f"{GetMetricPrec(metric_b, 0)}-{GetMetricPrec(metric_b, 1)}-{GetMetricPrec(metric_b, 2)}-{GetMetricPrec(metric_b, 3)}")
        #         print("-"*20)
        #         break

        def normalize(list_of_num):
            out = []
            min_v = min(list_of_num)
            max_v = max(list_of_num)
            for i in list_of_num:
                out.append(((i-min_v)/(max_v-min_v))*100)
            return out

        # normalize_ret = normalize(output_tmam)

        amd = min(min(temp_tmam),0)

        temp_new = [i + abs(amd) for i in temp_tmam]
        sum_new = temp_new[0] + temp_new[1] + temp_new[2] + temp_new[3]
        output_new = []

        # % 0 Retiring
        output_new.append(max(round(100 * (temp_new[0] / sum_new), ndigits=3),0))
        # % 1 Bad Spec
        output_new.append(max(round(100 * (
                1 - (temp_new[0] / sum_new) - (temp_new[2] / sum_new - uop_drop_delta / slot_delta) - (
                temp_new[3] / sum_new)), ndigits=3),0))
        # % 2 Front-End
        output_new.append(max(round(100 * (temp_new[2] / sum_new - uop_drop_delta / slot_delta), ndigits=3),0))
        # % 3 Back-End
        output_new.append(max(round(100 * (temp_new[3] / sum_new), ndigits=3),0))
        # % 4 Heavy Ops
        output_new.append(max(round(100 * (temp_new[4] / sum_new), ndigits=3),0))
        # % 5 Branch Mispred
        output_new.append(max(round(100 * (temp_new[5] / sum_new), ndigits=3),0))
        # % 6 Fetch Latency
        output_new.append(max(round(100 * (temp_new[6] / sum_new - uop_drop_delta / slot_delta), ndigits=3),0))
        # % 7 Memory Bound
        output_new.append(max(round(100 * (temp_new[7] / sum_new), ndigits=3),0))
        # % 8 Light Ops
        output_new.append(max(round(output_new[0] - output_new[4], ndigits=3),0))
        # % 9 Machine Clear
        output_new.append(max(round(output_new[1] - output_new[5], ndigits=3),0))
        # % 10 Fetch Bandwidth
        output_new.append(max(round(output_new[2] - output_new[6], ndigits=3),0))
        # % 11 Core Bound
        output_new.append(max(round(output_new[3] - output_new[7], ndigits=3),0))


        output_tmam.append(f"{slot_a}|{slot_b}|{slot_delta}")
        output_tmam.append(f"{temp_tmam[0]}|{temp_tmam[1]}|{temp_tmam[2]}|{temp_tmam[3]}")
        output_tmam.append(f"{output_new}")
        output_tmam.append(f"{uop_drop_a}|{uop_drop_b}|{uop_drop_delta}")


        return tmam_names, output_tmam

    # Topdown metrics calculation where the topdown metrics will be reset every time
    def CalTopDownWithoutDiff(self, slot_a, metric_a, uop_drop_a, slot_b, metric_b, uop_drop_b):

        def GetMetricPrec(metri, i):
            return (int(metri) >> (i * 8)) & 0xff

        # print(f"TopDown: {slot_a},{metric_a},{uop_drop_a},{slot_b},{metric_b},{uop_drop_b}")
        # slot_delta = slot_b - slot_a + 0.00001
        slot_delta = slot_b
        uop_drop_delta = uop_drop_b - uop_drop_a
        tmam_names = ['TMA_1_..Retiring(%)', 'TMA_1_..Bad Speculation(%)', 'TMA_1_..Front-End bound(%)',
                      'TMA_1_..Back-End Bound(%)',
                      'TMA_2_..Heavy ops(%)', 'TMA_2_..Branch Misprediction(%)', 'TMA_2_..Fetch Lat(%)',
                      'TMA_2_..Mem Bound(%)',
                      'TMA_2_..Light ops(%)', 'TMA_2_..Machine Clear(%)', 'TMA_2_..Fetch BW(%)',
                      'TMA_2_..Core Bound(%)']
        temp_tmam = []
        output_tmam = []
        # 0 Retiring
        temp_tmam.append(GetMetricPrec(metric_b, 0))
        # 1 bad speculation
        temp_tmam.append(GetMetricPrec(metric_b, 1))
        # 2 Front-End
        temp_tmam.append(GetMetricPrec(metric_b, 2))
        # 3 Back-End
        temp_tmam.append(GetMetricPrec(metric_b, 3))

        # 4 Heavy Ops
        temp_tmam.append(GetMetricPrec(metric_b, 4))
        # 5 Branch Mispred
        temp_tmam.append(GetMetricPrec(metric_b, 5))
        # 6 Fetch Latency
        temp_tmam.append(GetMetricPrec(metric_b, 6))
        # 7 Memory Bound
        temp_tmam.append(GetMetricPrec(metric_b, 7))

        sum_l1 = temp_tmam[0] + temp_tmam[1] + temp_tmam[2] + temp_tmam[3] + 0.00001
        # % 0 Retiring
        output_tmam.append(round(100 * (temp_tmam[0] / sum_l1), ndigits=3))
        # % 1 Bad Spec
        output_tmam.append(round(100 * (
                1 - (temp_tmam[0] / sum_l1) - (temp_tmam[2] / sum_l1 - uop_drop_delta / slot_delta) - (
                temp_tmam[3] / sum_l1)), ndigits=3))
        # % 2 Front-End
        output_tmam.append(round(100 * (temp_tmam[2] / sum_l1 - uop_drop_delta / slot_delta), ndigits=3))
        # % 3 Back-End
        output_tmam.append(round(100 * (temp_tmam[3] / sum_l1), ndigits=3))
        # % 4 Heavy Ops
        output_tmam.append(round(100 * (temp_tmam[4] / sum_l1), ndigits=3))
        # % 5 Branch Mispred
        output_tmam.append(round(100 * (temp_tmam[5] / sum_l1), ndigits=3))
        # % 6 Fetch Latency
        output_tmam.append(round(100 * (temp_tmam[6] / sum_l1 - uop_drop_delta / slot_delta), ndigits=3))
        # % 7 Memory Bound
        output_tmam.append(round(100 * (temp_tmam[7] / sum_l1), ndigits=3))
        # % 8 Light Ops
        output_tmam.append(round(output_tmam[0] - output_tmam[4], ndigits=3))
        # % 9 Machine Clear
        output_tmam.append(round(output_tmam[1] - output_tmam[5], ndigits=3))
        # % 10 Fetch Bandwidth
        output_tmam.append(round(output_tmam[2] - output_tmam[6], ndigits=3))
        # % 11 Core Bound
        output_tmam.append(round(output_tmam[3] - output_tmam[7], ndigits=3))

        return tmam_names, output_tmam

    # Calculate topdown metrics and scale into a proper range
    def CalTopDownByDiffNew(self, slot_a, metric_a, uop_drop_a, slot_b, metric_b, uop_drop_b):

        def GetMetricPrec(metri, i):
            return (int(metri) >> (i * 8)) & 0xff

        # print(f"TopDown: {slot_a},{metric_a},{uop_drop_a},{slot_b},{metric_b},{uop_drop_b}")
        slot_delta = slot_b - slot_a + 0.00001
        uop_drop_delta = uop_drop_b - uop_drop_a
        tmam_names = ['TMA_1_..Retiring(%)', 'TMA_1_..Bad Speculation(%)', 'TMA_1_..Front-End bound(%)',
                      'TMA_1_..Back-End Bound(%)',
                      'TMA_2_..Heavy ops(%)', 'TMA_2_..Branch Misprediction(%)', 'TMA_2_..Fetch Lat(%)',
                      'TMA_2_..Mem Bound(%)',
                      'TMA_2_..Light ops(%)', 'TMA_2_..Machine Clear(%)', 'TMA_2_..Fetch BW(%)',
                      'TMA_2_..Core Bound(%)', 'SlotsA,B,Diff', 'Scaled:', 'Output_new:']
        temp_tmam = []
        output_tmam = []
        # 0 Retiring
        temp_tmam.append(
            (GetMetricPrec(metric_b, 0) * slot_b - GetMetricPrec(metric_a, 0) * slot_a) / slot_delta)
        # 1 bad speculation
        temp_tmam.append(
            (GetMetricPrec(metric_b, 1) * slot_b - GetMetricPrec(metric_a, 1) * slot_a) / slot_delta)
        # 2 Front-End
        temp_tmam.append(
            (GetMetricPrec(metric_b, 2) * slot_b - GetMetricPrec(metric_a, 2) * slot_a) / slot_delta)
        # 3 Back-End
        temp_tmam.append(
            (GetMetricPrec(metric_b, 3) * slot_b - GetMetricPrec(metric_a, 3) * slot_a) / slot_delta)

        # 4 Heavy Ops
        temp_tmam.append(
            (GetMetricPrec(metric_b, 4) * slot_b - GetMetricPrec(metric_a, 4) * slot_a) / slot_delta)
        # 5 Branch Mispred
        temp_tmam.append(
            (GetMetricPrec(metric_b, 5) * slot_b - GetMetricPrec(metric_a, 5) * slot_a) / slot_delta)
        # 6 Fetch Latency
        temp_tmam.append(
            (GetMetricPrec(metric_b, 6) * slot_b - GetMetricPrec(metric_a, 6) * slot_a) / slot_delta)
        # 7 Memory Bound
        temp_tmam.append(
            (GetMetricPrec(metric_b, 7) * slot_b - GetMetricPrec(metric_a, 7) * slot_a) / slot_delta)

        amd = min(min(temp_tmam),0)

        temp_new = [i + abs(amd) for i in temp_tmam]
        sum_new = temp_new[0] + temp_new[1] + temp_new[2] + temp_new[3]
        output_new = []

        # % 0 Retiring
        output_new.append(max(round(100 * (temp_new[0] / sum_new), ndigits=3),0))
        # % 1 Bad Spec
        output_new.append(max(round(100 * (
                1 - (temp_new[0] / sum_new) - (temp_new[2] / sum_new - uop_drop_delta / slot_delta) - (
                temp_new[3] / sum_new)), ndigits=3),0))
        # % 2 Front-End
        output_new.append(max(round(100 * (temp_new[2] / sum_new - uop_drop_delta / slot_delta), ndigits=3),0))
        # % 3 Back-End
        output_new.append(max(round(100 * (temp_new[3] / sum_new), ndigits=3),0))
        # % 4 Heavy Ops
        output_new.append(max(round(100 * (temp_new[4] / sum_new), ndigits=3),0))
        # % 5 Branch Mispred
        output_new.append(max(round(100 * (temp_new[5] / sum_new), ndigits=3),0))
        # % 6 Fetch Latency
        output_new.append(max(round(100 * (temp_new[6] / sum_new - uop_drop_delta / slot_delta), ndigits=3),0))
        # % 7 Memory Bound
        output_new.append(max(round(100 * (temp_new[7] / sum_new), ndigits=3),0))
        # % 8 Light Ops
        output_new.append(max(round(output_new[0] - output_new[4], ndigits=3),0))
        # % 9 Machine Clear
        output_new.append(max(round(output_new[1] - output_new[5], ndigits=3),0))
        # % 10 Fetch Bandwidth
        output_new.append(max(round(output_new[2] - output_new[6], ndigits=3),0))
        # % 11 Core Bound
        output_new.append(max(round(output_new[3] - output_new[7], ndigits=3),0))

        output_new.append(f"{slot_a}-{slot_b}-{slot_delta}")
        output_new.append(f"{temp_tmam[0]}-{temp_tmam[1]}-{temp_tmam[2]}-{temp_tmam[3]}")


        return tmam_names, output_new
