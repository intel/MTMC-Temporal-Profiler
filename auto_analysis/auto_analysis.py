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

#!/usr/bin/python
import json
import networkx as nx
import pandas as pd
import numpy as np
import math

import argparse
import parser as ps
import os
import sys

# data class for tmam data
class TmamData(object) :
    def __init__(self, retiring, bad_speculation, front_end_bound, back_end_bound, branch_misperd, machine_clears, fetch_latency, fetch_bandwidth, memory_bound, core_bound, l1_bound = 0.0, l2_bound = 0.0, l3_bound = 0.0, dram_bound = 0.0, store_bound = 0.0) :
        self.__level1_retiring = retiring
        self.__level1_bad_speculation = bad_speculation
        self.__level1_front_end_bound = front_end_bound
        self.__level1_back_end_bound = back_end_bound
        self.__level2_branch_misperd = branch_misperd
        self.__level2_machine_clears = machine_clears
        self.__level2_fetch_latency = fetch_latency
        self.__level2_fetch_bandwidth = fetch_bandwidth
        self.__level2_memory_bound = memory_bound
        self.__level2_core_bound = core_bound
        self.__level3_l1_bound = l1_bound
        self.__level3_l2_bound = l2_bound
        self.__level3_l3_bound = l3_bound
        self.__level3_dram_bound = dram_bound
        self.__level3_store_bound = store_bound

    def GetLevel1Retiring(self) :
        return self.__level1_retiring

    def SetLevel1Retiring(self, level1_retiring) :
        self.__level1_retiring = level1_retiring

    def GetLevel1BadSpeculation(self) :
        return self.__level1_bad_speculation

    def SetLevel1BadSpeculation(self, level1_bad_speculation) :
        self.__level1_bad_speculation = level1_bad_speculation

    def GetLevel1FrontEndBound(self) :
        return self.__level1_front_end_bound

    def SetLevel1FrontEndBound(self, level1_front_end_bound) :
        self.__level1_front_end_bound = level1_front_end_bound

    def GetLevel1BackEndBound(self) :
        return self.__level1_back_end_bound

    def SetLevel1BackEndBound(self, level1_back_end_bound) :
        self.__level1_back_end_bound = level1_back_end_bound

    def GetLevel2BranchMisperd(self) :
        return self.__level2_branch_misperd

    def SetLevel2BranchMisperd(self, level2_branch_misperd) :
        self.__level2_branch_misperd = level2_branch_misperd

    def GetLevel2MachineClears(self) :
        return self.__level2_machine_clears

    def SetLevel2MachineClears(self, level2_machine_clears) :
        self.__level2_machine_clears = level2_machine_clears

    def GetLevel2FetchLatency(self) :
        return self.__level2_fetch_latency

    def SetLevel2FetchLatency(self, level2_fetch_latency) :
        self.__level2_fetch_latency = level2_fetch_latency

    def GetLevel2FetchBandwidth(self) :
        return self.__level2_fetch_bandwidth

    def SetLevel2FetchBandwidth(self, level2_fetch_bandwidth) :
        self.__level2_fetch_bandwidth = level2_fetch_bandwidth

    def GetLevel2MemoryBound(self) :
        return self.__level2_memory_bound

    def SetLevel2MemoryBound(self, level2_memory_bound) :
        self.__level2_memory_bound = level2_memory_bound

    def GetLevel2CoreBound(self) :
        return self.__level2_core_bound

    def SetLevel2CoreBound(self, level2_core_bound) :
        self.__level2_core_bound = level2_core_bound

    def GetLevel3L1Bound(self) :
        return self.__level3_l1_bound

    def SetLevel3L1Bound(self, level3_l1_bound) :
        self.__level3_l1_bound = level3_l1_bound

    def GetLevel3L2Bound(self) :
        return self.__level3_l2_bound

    def SetLevel3L2Bound(self, level3_l2_bound) :
        self.__level3_l2_bound = level3_l2_bound

    def GetLevel3L3Bound(self) :
        return self.__level3_l3_bound

    def SetLevel3L3Bound(self, level3_l3_bound) :
        self.__level3_l3_bound = level3_l3_bound

    def GetLevel3DramBound(self) :
        return self.__level3_dram_bound

    def SetLevel3DramBound(self, level3_dram_bound) :
        self.__level3_dram_bound = level3_dram_bound

    def GetLevel3StoreBound(self) :
        return self.__level3_store_bound

    def SetLevel3StoreBound(self, level3_store_bound) :
        self.__level3_store_bound = level3_store_bound

class DistributionData(object) :
    def __init__(self) :
        self.__d_data = {}
        self.__raw_data = []
        self.__times = 0
        self.__p_d_data = []
        self.__slot_num = 100
        for i in range(self.__slot_num) :
            self.__p_d_data.append(0.0)

        self.__p_x_min = 0.0
        self.__p_x_max = 0.0

    def AddData(self, x_i) :
        x = x_i * 1.0
        if x in self.__d_data.keys() :
            self.__d_data[x] = self.__d_data[x] + 1
        else :
            self.__d_data[x] = 1
        self.__raw_data.append(x)
        self.__times = self.__times + 1

    def GenProbabilityDistribution(self) :
        u = self.GetUnit()
        x_min = self.__p_x_min

        for k in self.__d_data.keys() :
            if u > 0 :
                index =  int((k - x_min) / u)
            else :
                index = 0
            self.__p_d_data[index] = self.__p_d_data[index] + self.__d_data[k]

        for i in range(self.__slot_num) :
            self.__p_d_data[index] = self.__p_d_data[index] / self.__times

    def GetUnit(self) :
        x_min = self.__p_x_min
        x_max = self.__p_x_max
        return (x_max - x_min) * 1.0 / (self.__slot_num - 1)

    def GetYValue(self, x) :
        if x in self.__d_data.keys() :
            return self.__d_data[x]
        else :
            return 0

    def GetYPValue(self, x) :
        return self.__p_d_data[x]

    def SetPXMin(self, x_min) :
        self.__p_x_min = x_min

    def SetPXMax(self, x_max) :
        self.__p_x_max = x_max

    def GetSlotNum(self) :
        return self.__slot_num

    def GetXMin(self) :
        return min(self.__d_data.keys())

    def GetXMax(self) :
        return max(self.__d_data.keys())

    def GetRawData(self) :
        return self.__raw_data

class TmamDataDistribution(object) :
    def __init__(self, retiring_d, bad_speculation_d, front_end_bound_d, back_end_bound_d, branch_misperd_d, machine_clears_d, fetch_latency_d, fetch_bandwidth_d, memory_bound_d, core_bound_d, cache_l1_bound_d, cache_l2_bound_d, cache_l3_bound_d, dram_bound_d, store_bound_d) :
        self.__level1_retiring_d = retiring_d
        self.__level1_bad_speculation_d = bad_speculation_d
        self.__level1_front_end_bound_d = front_end_bound_d
        self.__level1_back_end_bound_d = back_end_bound_d
        self.__level2_branch_misperd_d = branch_misperd_d
        self.__level2_machine_clears_d = machine_clears_d
        self.__level2_fetch_latency_d = fetch_latency_d
        self.__level2_fetch_bandwidth_d = fetch_bandwidth_d
        self.__level2_memory_bound_d = memory_bound_d
        self.__level2_core_bound_d = core_bound_d
        self.__level3_l1_bound_d = cache_l1_bound_d
        self.__level3_l2_bound_d = cache_l2_bound_d
        self.__level3_l3_bound_d = cache_l3_bound_d
        self.__level3_dram_bound_d = dram_bound_d
        self.__level3_store_bound_d = store_bound_d

    def GetLevel1RetiringD(self) :
        return self.__level1_retiring_d

    def GetLevel1BadSpeculationD(self) :
        return self.__level1_bad_speculation_d

    def GetLevel1FrontEndBoundD(self) :
        return self.__level1_front_end_bound_d

    def GetLevel1BackEndBoundD(self) :
        return self.__level1_back_end_bound_d

    def GetLevel2BranchMisperdD(self) :
        return self.__level2_branch_misperd_d

    def GetLevel2MachineClearsD(self) :
        return self.__level2_machine_clears_d

    def GetLevel2FetchLatencyD(self) :
        return self.__level2_fetch_latency_d

    def GetLevel2FetchBandwidthD(self) :
        return self.__level2_fetch_bandwidth_d

    def GetLevel2MemoryBoundD(self) :
        return self.__level2_memory_bound_d

    def GetLevel2CoreBoundD(self) :
        return self.__level2_core_bound_d

    def GetLevel3L1BoundD(self) :
        return self.__level3_l1_bound_d

    def GetLevel3L2BoundD(self) :
        return self.__level3_l2_bound_d

    def GetLevel3L3BoundD(self) :
        return self.__level3_l3_bound_d

    def GetLevel3DramBoundD(self) :
        return self.__level3_dram_bound_d

    def GetLevel3StoreBoundD(self) :
        return self.__level3_store_bound_d

    def AddData(self, tmam) :
        self.__level1_retiring_d.AddData(tmam.GetLevel1Retiring())
        self.__level1_bad_speculation_d.AddData(tmam.GetLevel1BadSpeculation())
        self.__level1_front_end_bound_d.AddData(tmam.GetLevel1FrontEndBound())
        self.__level1_back_end_bound_d.AddData(tmam.GetLevel1BackEndBound())
        self.__level2_branch_misperd_d.AddData(tmam.GetLevel2BranchMisperd())
        self.__level2_machine_clears_d.AddData(tmam.GetLevel2MachineClears())
        self.__level2_fetch_latency_d.AddData(tmam.GetLevel2FetchLatency())
        self.__level2_fetch_bandwidth_d.AddData(tmam.GetLevel2FetchBandwidth())
        self.__level2_memory_bound_d.AddData(tmam.GetLevel2MemoryBound())
        self.__level2_core_bound_d.AddData(tmam.GetLevel2CoreBound())
        self.__level3_l1_bound_d.AddData(tmam.GetLevel3L1Bound())
        self.__level3_l2_bound_d.AddData(tmam.GetLevel3L2Bound())
        self.__level3_l3_bound_d.AddData(tmam.GetLevel3L3Bound())
        self.__level3_dram_bound_d.AddData(tmam.GetLevel3DramBound())
        self.__level3_store_bound_d.AddData(tmam.GetLevel3StoreBound())


class KlDivergence(object) :
    def __init__(self, a_distribution, b_distribution) :
        self.__a_distribution = a_distribution
        self.__b_distribution = b_distribution
        x_min = min(self.__a_distribution.GetXMin(), self.__b_distribution.GetXMin())
        x_max = max(self.__a_distribution.GetXMax(), self.__b_distribution.GetXMax())
        self.__b_distribution.SetPXMax(x_max)
        self.__b_distribution.SetPXMin(x_min)
        self.__a_distribution.SetPXMax(x_max)
        self.__a_distribution.SetPXMin(x_min)
        self.__b_distribution.GenProbabilityDistribution()
        self.__a_distribution.GenProbabilityDistribution()

    def Compute(self) :
        value = 0.0
        slot_num = self.__a_distribution.GetSlotNum()
        for i in range(slot_num):
            value += self.__a_distribution.GetYPValue(i) * 1.0 * math.log(self.__a_distribution.GetYPValue(i)/self.__b_distribution.GetYPValue(i))
        return value

class JsDivergence(object) :
    def __init__(self, a_distribution, b_distribution) :
        self.__a_distribution = a_distribution
        self.__b_distribution = b_distribution
        x_min = min(self.__a_distribution.GetXMin(), self.__b_distribution.GetXMin())
        x_max = max(self.__a_distribution.GetXMax(), self.__b_distribution.GetXMax())
        self.__b_distribution.SetPXMax(x_max)
        self.__b_distribution.SetPXMin(x_min)
        self.__a_distribution.SetPXMax(x_max)
        self.__a_distribution.SetPXMin(x_min)
        self.__b_distribution.GenProbabilityDistribution()
        self.__a_distribution.GenProbabilityDistribution()

    def Compute(self) :
        value = 0.0
        slot_num = self.__a_distribution.GetSlotNum()
        for i in range(slot_num):
            a_y_value = self.__a_distribution.GetYPValue(i)
            b_y_value = self.__b_distribution.GetYPValue(i)
            if a_y_value > 1e-6 :
                value += 0.5 * a_y_value * math.log(a_y_value * 2.0/(a_y_value + b_y_value))
            if b_y_value > 1e-6 :
                value += 0.5 * b_y_value * math.log(b_y_value * 2.0 /(a_y_value + b_y_value))
        return value

# data class for profile data
class ProfileData(object) :
    def __init__(self, name, op, start_time, dur, tid, tmam=None, children_list=[], input_name_list=[]) :
        self.__name = name
        self.__op = op
        self.__start_time = start_time
        self.__dur = dur
        self.__tid = tid
        self.__tmam = tmam
        self.__children_list = children_list
        self.__input_name_list = input_name_list

    def GetName(self) :
        return self.__name

    def GetOp(self) :
        return self.__op

    def SetOp(self, op) :
        self.__op = op

    def GetStartTime(self) :
        return self.__start_time

    def GetDur(self) :
        return self.__dur

    def GetTmam(self) :
        return self.__tmam

    def GetTid(self) :
        return self.__tid

    def GetChildrenList(self) :
        return self.__children_list

    def SetChildrenList(self, children_list) :
        self.__children_list = children_list

    def GetInputNameList(self) :
        return self.__input_name_list

    def SetInputNameList(self, input_name_list) :
        self.__input_name_list = input_name_list

    def GetTotalArea(self) :
        total_area = 0.0
        total_area = total_area + self.__dur
        for child in self.__children_list :
            total_area = total_area + child.GetTotalArea()
        return total_area

    def GetAreaBtwStartEnd(self, start_time, end_time) :
        total_area = 0.0
        start_t = self.__start_time
        end_t = self.__start_time + self.__dur
        start_t = max(start_t, start_time)
        end_t = min(end_t, end_time)
        area = (end_t - start_t) if (end_t - start_t) >= 0 else 0
        total_area = total_area + area
        for child in self.__children_list :
            total_area = total_area + child.GetAreaBtwStartEnd(start_time, end_time)
        return total_area

class ProfileDataSummary(object) :
    def __init__(self, name, op) :
        self.__name = name
        self.__op = op
        self.__total_area = 0.0
        self.__op_util_ratio_with_intra = []

    def GetName(self) :
        return self.__name

    def GetOp(self) :
        return self.__op

    def SetName(self, name) :
        self.__name = name

    def GetTotalArea(self) :
        return self.__total_area

    def SetTotalData(self, total_area) :
        self.__total_area = total_area

    def GetOpUtilRatioWithIntra(self) :
        return self.__op_util_ratio_with_intra

    def SetOpUtilRatioWithIntra(self, op_util_ratio_with_intra) :
        self.__op_util_ratio_with_intra = op_util_ratio_with_intra

class CpuUtilData(object) :
    def __init__(self, input_file) :
        pass

    def GetCpuUtil(self, start_time, end_time) :
        ret = 0.7
        return ret

# the class for loading timeline data
class TimelineData(object) :
    # init the timeline with the input file path
    def __init__(self, input_file) :
        self.__input_file_path = input_file
        self.__eigen_prof = []
        self.__cpu_mapping = []
        self.__ops = {}
        self.__detail_info = []
        self.__groundtruth_info = []
        self.__groundtruth_tmam_d = {}
        self.__data = {}
        self.__tmam_distribution = {}
        self.__qps_area_distribution = {}
        self.__latency_distribution = {}

        self.__load()
        print('load data done:' + self.__input_file_path)
        self.ProcessEigenProf()
        print('process data done.')
        self.ProcessDistribution()
        print('process distribution done.')

    # load data from input file
    def __load(self) :
        with open(self.__input_file_path, 'r') as f :
            d = json.load(f)
            for elem in d :
                if 'cat' in elem :
                    if elem['cat'] == 'MTMC logs' :
                        self.__eigen_prof.append(elem)
                    elif elem['cat'] == 'CPU Mapping' :
                        self.__cpu_mapping.append(elem)
                    elif elem['cat'] == 'Op' :
                        if ('args' in elem) and ('name' in elem['args']) :
                            self.__ops[elem['args']['name']] = elem
                    if elem['cat'] == "Detailed node state" :
                        self.__detail_info.append(elem)

    def GetGroundtruthTmamD(self, name) :
        #print(name)
        return self.__groundtruth_tmam_d[name]

    # return the raw data of eigen prof
    def GetEigenProf(self) :
        return self.__eigen_prof

    # get the raw data of cpu mapping
    def GetCpuMapping(self) :
        return self.__cpu_mapping

    # get the raw data of ops, including graph information for each op
    def GetOps(self) :
        return self.__ops

    # get data with parent/children mapping information
    def GetData(self) :
        return self.__data

    # get tmam distribution information
    def GetTmamDistribution(self) :
        return self.__tmam_distribution

    # get area distribution information
    def GetQpsAreaDistribution(self) :
        return self.__qps_area_distribution

    # get latency distribution information
    def GetLatencyDistribution(self) :
        return self.__latency_distribution

    def ProcessDistribution(self) :
        for step_id in self.__data.keys() :
            for e in self.__data[step_id] :
                name = e.GetName()
                dur = e.GetDur()
                qps_area = e.GetTotalArea()

                # build tmam distribution
                for c in e.GetChildrenList() :
                    tmam = c.GetTmam()
                    if name in self.__tmam_distribution.keys() :
                        tmam_d = self.__tmam_distribution[name]
                        tmam_d.AddData(tmam)
                    else :
                        retiring_d = DistributionData()
                        bad_speculation_d = DistributionData()
                        front_end_bound_d = DistributionData()
                        back_end_bound_d = DistributionData()
                        branch_misperd_d = DistributionData()
                        machine_clears_d = DistributionData()
                        fetch_latency_d = DistributionData()
                        fetch_bandwidth_d = DistributionData()
                        memory_bound_d = DistributionData()
                        core_bound_d = DistributionData()
                        l1_bound_d = DistributionData()
                        l2_bound_d = DistributionData()
                        l3_bound_d = DistributionData()
                        dram_bound_d = DistributionData()
                        store_bound_d = DistributionData()
                        tmam_d = TmamDataDistribution(retiring_d, bad_speculation_d, front_end_bound_d, back_end_bound_d, branch_misperd_d, machine_clears_d, fetch_latency_d, fetch_bandwidth_d, memory_bound_d, core_bound_d, l1_bound_d, l2_bound_d, l3_bound_d, dram_bound_d, store_bound_d)
                        tmam_d.AddData(tmam)
                        self.__tmam_distribution[name] = tmam_d

                # build area distribution
                if name in self.__qps_area_distribution.keys() :
                    qps_area_d = self.__qps_area_distribution[name]
                    qps_area_d.AddData(qps_area)
                else :
                    qps_area_d = DistributionData()
                    qps_area_d.AddData(qps_area)
                    self.__qps_area_distribution[name] = qps_area_d

                # build latency distribution
                if name in self.__latency_distribution.keys() :
                    latency_d = self.__latency_distribution[name]
                    latency_d.AddData(dur)
                else :
                    latency_d = DistributionData()
                    latency_d.AddData(dur)
                    self.__latency_distribution[name] = latency_d

    # process raw data of eigen_prof and generate the parent/children mapping information
    def ProcessEigenProf(self) :
        name_list = {}
        help_dict = {}
        for elem in self.__detail_info :
            if ('args' in elem) and ('step_id' in elem['args']) and ('node_name' in elem['args']):
                step_id = int(elem['args']['step_id'])
                if step_id == -1 :
                    continue
                op_type = elem['name']
                tid = int(elem['tid'])
                start_time = float(elem['ts'])
                dur = float(elem['dur'])
                name = elem['args']['node_name']

                if step_id not in name_list :
                    name_list[step_id] = []
                if name in name_list[step_id] :
                    continue
                input_name_list = list()

                if 'input_nodes' in elem['args'].keys():
                    input_name_list = elem['args']['input_nodes']
                elif name in self.__ops and ('args' in elem) :
                    d = self.__ops[name]['args']
                    index = 0
                    while True :
                        input_key = 'input' + str(index)
                        if input_key in d :
                            input_name_list.append(d[input_key])
                        else :
                            break
                        index = index + 1
                d = ProfileData(name, op_type, start_time, dur, tid,  None, list(), input_name_list)
                name_list[step_id].append(name)
                if step_id in self.__data :
                    self.__data[step_id].append(d)
                    help_dict[step_id][name] = d
                else :
                    self.__data[step_id] = []
                    self.__data[step_id].append(d)
                    help_dict[step_id] = {}
                    help_dict[step_id][name] = d

        print('process inter data done.')
        for elem in self.__eigen_prof :
            if ('args' in elem) and ('step_id' in elem['args']) :
                step_id = int(elem['args']['step_id'])
                if step_id == -1 :
                    continue
                op_type = elem['name']
                start_time = float(elem['ts'])
                dur = float(elem['dur'])
                tid = int(elem['tid'])
                name = elem['args']['node_name']
                #tmam data
                retiring = float(elem['args']['Met-.metric_TMA_Retiring(%)'])
                bad_speculation = float(elem['args']['Met-.metric_TMA_Bad_Speculation(%)'])
                front_end_bound = float(elem['args']['Met-.metric_TMA_Frontend_Bound(%)'])
                back_end_bound = float(elem['args']['Met-.metric_TMA_Backend_Bound(%)'])
                branch_misperd = float(elem['args']['Met-.metric_TMA_..Branch_Mispredicts(%)'])
                machine_clears = float(elem['args']['Met-.metric_TMA_..Machine_Clears(%)'])
                fetch_latency = float(elem['args']['Met-.metric_TMA_..Fetch_Latency(%)'])
                fetch_bandwidth = float(elem['args']['Met-.metric_TMA_..Fetch_Bandwidth(%)'])
                memory_bound = float(elem['args']['Met-.metric_TMA_..Memory_Bound(%)'])
                core_bound = float(elem['args']['Met-.metric_TMA_..Core_Bound(%)'])
                l1_bound = float(elem['args']['Met-.metric_TMA_....L1_Bound(%)'])
                l2_bound = float(elem['args']['Met-.metric_TMA_....L2_Bound(%)'])
                l3_bound = float(elem['args']['Met-.metric_TMA_....L3_Bound(%)'])
                dram_bound = float(elem['args']['Met-.metric_TMA_....DRAM_Bound(%)'])
                store_bound = float(elem['args']['Met-.metric_TMA_....Store_Bound(%)'])

                t = TmamData(retiring, bad_speculation, front_end_bound, back_end_bound, branch_misperd, machine_clears, fetch_latency, fetch_bandwidth, memory_bound, core_bound, l1_bound, l2_bound, l3_bound, dram_bound, store_bound)
                d = ProfileData(name, op_type, start_time, dur, tid, t, list(), list())
                if name in help_dict[step_id] :
                    e = help_dict[step_id][name]
                    children_list = e.GetChildrenList()
                    children_list.append(d)
                    d.SetOp(e.GetOp())
                    e.SetChildrenList(children_list)
                #for e in self.__data[step_id] :
                #    if e.GetName() == name :
                #        children_list = e.GetChildrenList()
                #        children_list.append(d)
                #        d.SetOp(e.GetOp())
                #        e.SetChildrenList(children_list)
        print('process intra data done.')

    # get the list of step id in the eigen prof data
    def GetStepIdList(self) :
        step_id_list = []
        for elem in self.__eigen_prof :
            if ('args' in elem) and ('step_id' in elem['args']) :
                step_id = int(elem['args']['step_id'])
                if step_id == -1 :
                    continue
                step_id_list.append(step_id)

        return list(set(step_id_list))

    # get the list of step id in the eigen prof data
    def GetOpNameList(self) :
        op_name_list = []
        for elem in self.__eigen_prof :
            if ('args' in elem) and ('step_id' in elem['args']) :
                op_name_list.append(elem['args']['node_name'])

        return list(set(op_name_list))

    # handle the data, and do average for each step, then return the average data
    def AverageData(self, step_id_list, op_name_list) :
        aver_data = {}
        tmp_data = {}
        intra_threads_num = 14
        for step in step_id_list :
            for name in op_name_list :
                for elem in self.__data[step] :
                    if (elem.GetName()) == name and (elem.GetDur() > 0) :
                        start_t = elem.GetStartTime()
                        op = elem.GetOp()
                        end_t = start_t + elem.GetDur()
                        total_area = elem.GetAreaBtwStartEnd(start_t, end_t)
                        op_util_ratio_with_intra = total_area / (intra_threads_num * (end_t - start_t))
                        if name not in tmp_data :
                            tmp_data[name] = []
                            tmp_data[name].append((op, total_area, op_util_ratio_with_intra))
                        else :
                            tmp_data[name].append((op, total_area, op_util_ratio_with_intra))

        for k in tmp_data.keys() :
            v = tmp_data[k]
            aver_total_area = 0.0
            aver_op_util_ratio_with_intra = 0.0
            size = len(v)
            op = None
            if size > 0 :
                for e in v :
                    op = e[0]
                    aver_total_area = aver_total_area + e[1]
                    aver_op_util_ratio_with_intra = aver_op_util_ratio_with_intra + e[2]
                aver_total_area = aver_total_area / size
                aver_op_util_ratio_with_intra = aver_op_util_ratio_with_intra / size
            summary = ProfileDataSummary(k, op)
            summary.SetTotalData(aver_total_area)
            summary.SetOpUtilRatioWithIntra(aver_op_util_ratio_with_intra)
            aver_data[k] = summary

        return aver_data

    def SortTotalArea(self, step_id_list) :
        ret = {}
        for step in step_id_list :
            l = self.__data[step]
            l.sort(key = lambda t: t.GetTotalArea(), reverse=True)
            ret[step] = l
        return ret

    def SortTotalAreaWithLimitation(self, step_id_list, name_info) :
        ret = {}
        for step in step_id_list :
            l = self.__data[step]
            l_temp = list()
            for n in name_info[step] :
                for e in l :
                    if e.GetName() == n :
                        l_temp.append(e)
            l_temp.sort(key = lambda t: t.GetTotalArea(), reverse=True)
            ret[step] = l_temp
        return ret

    def GetDataWithStep(self, step_id) :
        return self.__data[step_id]

    def FindCriticalPath(self) :
        ret = {}
        for step in self.__data.keys() :
            in_d = list()
            num = 0
            for e in self.__data[step] :
                to_n = e.GetName()
                cost_d = e.GetDur()
                for i_n in e.GetInputNameList() :
                    from_n = i_n
                    in_d.append((from_n, to_n, {'cost':cost_d}))
                if len(e.GetInputNameList()) == 0 :
                    from_n = "virtual_input_place_holder_" + str(num)
                    in_d.append((from_n, to_n, {'cost':cost_d}))
                    num = num + 1
            dg = nx.DiGraph(in_d)
            ret[step] = nx.dag_longest_path(dg, weight="cost")[1:]
        return ret

    def GetThreadsNum(self) :
        # todo: get from the log file.
        return 48

    def GetAllOpsAreaBtwStartEnd(self, step_id, start_time, end_time) :
        area = 0.0
        for e in self.__data[step_id] :
            area = area + e.GetAreaBtwStartEnd(start_time, end_time)
        return area

# the class supports auto analysis function using timeline data as input
class AutoAnalysis(object) :

    # init the auto analysis with input data
    def __init__(self, target, timeline_data, groundtruth_timeline_data, cpu_util_data, output_dir) :
        self.__target = target
        self._timeline_data = timeline_data
        self._groundtruth_timeline_data = groundtruth_timeline_data
        self._cpu_util_data = cpu_util_data
        self._hotspot_list = {}
        self._hotspot_name_list = []
        self._output_dir = output_dir

    # get the target: QPS or Latency
    def target(self) :
        return self.__target

    # main function for analysis
    def Analysis(self) :
        self._generate_hotspot_list()
        # self._schedule_analysis()
        self._groundtruth_analysis()

    # generate the hotspot list with order, the strategy should be difference by target
    def _generate_hotspot_list(self) :
        pass

    def _generate_suggestions(self, filter_info , output_file_name) :
        t = self._timeline_data
        l3_store_node_names = []
        l3_store_info = {}
        l3_dram_node_names = []
        l3_dram_info = {}
        l3_l3_node_names = []
        l3_l3_info = {}
        fusion_node_names = []
        cat_node_names = []
        t_tmam_d = t.GetTmamDistribution()
        for name in self._hotspot_name_list :
            tmam_d = t_tmam_d[name]
            l3_store = round(np.percentile(tmam_d.GetLevel3StoreBoundD().GetRawData(), 50), 2)
            l3_dram =  round(np.percentile(tmam_d.GetLevel3DramBoundD().GetRawData(), 50), 2)
            l3_l3 =  round(np.percentile(tmam_d.GetLevel3L3BoundD().GetRawData(), 50), 2)
            if (l3_store > 25) and (name in filter_info) :
                l3_store_node_names.append(name)
                l3_store_info[name] = l3_store
            if (l3_dram > 25) and (name in filter_info) :
                l3_dram_node_names.append(name)
                l3_dram_info[name] = l3_dram
            if (l3_l3 > 25) and (name in filter_info) :
                l3_l3_node_names.append(name)
                l3_l3_info[name] = l3_l3
        step_id_list = self._timeline_data.GetStepIdList()
        data = self._timeline_data.GetData()
        step_id = step_id_list[int(len(step_id_list)/2)]
        for name in l3_dram_node_names :
            for e in data[step_id] :
                if name == e.GetName() :
                    for input_name in e.GetInputNameList() :
                        if input_name in l3_store_node_names :
                            for elem in data[step_id] :
                                if input_name == elem.GetName() :
                                    if elem.GetOp() != "_MklReshape" and elem.GetOp() != "Reshape" :
                                        fusion_node_names.append((input_name,name))
                        for elem in data[step_id] :
                            if input_name == elem.GetName() :
                                if elem.GetOp() == "_MklReshape" or elem.GetOp() == "Reshape" :
                                    for i in elem.GetInputNameList() :
                                        if i in l3_store_node_names :
                                            fusion_node_names.append((i,name))

        for name in l3_dram_node_names :
            for step_id in step_id_list :
                for e in data[step_id] :
                    if name == e.GetName() :
                        s_t = e.GetStartTime()
                        dur = e.GetDur()
                        for n in l3_l3_node_names :
                            for elem in data[step_id] :
                                if n == elem.GetName() :
                                    if (s_t > elem.GetStartTime() and s_t < (elem.GetStartTime() + elem.GetDur())) \
                                            or (elem.GetStartTime() > s_t and elem.GetStartTime() < (s_t + dur)) :
                                        cat_node_names.append((name, n))
        suggestions = []
        #fusion_node_names = list(set(fusion_node_names))
        for e in fusion_node_names :
            c = "fusion suggestion : (" + e[0] + " l3 store bound -- " + str(l3_store_info[e[0]]) + "% ," + e[1] + " l3 dram bound -- " + str(l3_dram_info[e[1]]) +"% )"
            suggestions.append(c)

        for e in l3_dram_node_names :
            c = "enable low precision(f16/bf16) : " + e + " l3 dram bound -- " + str(l3_dram_info[e]) + "%"
            suggestions.append(c)

        for e in l3_store_node_names :
            c = "enable low precision(f16/bf16) : " + e + " l3 store bound -- " + str(l3_store_info[e]) + "%"
            suggestions.append(c)

        cat_node_names = list(set(cat_node_names))
        for e in cat_node_names :
            c = "RDT optimization for: (" + e[0] + "," + e[1] + ")"
            suggestions.append(c)
        d = pd.DataFrame(np.array(suggestions))
        d.columns = ['suggestions']
        output_file = os.path.join(self._output_dir, output_file_name)
        d.to_csv(output_file)

    # schedule analysis function
    def _schedule_analysis(self) :
        list_info = []
        step_id_list = self._timeline_data.GetStepIdList()
        op_name_list = self._timeline_data.GetOpNameList()
        threads_num = self._timeline_data.GetThreadsNum()
        intra_threads_num = 14
        summary_data = self._timeline_data.AverageData(step_id_list, op_name_list)
        for step in step_id_list :
            for elem in self._hotspot_list[step] :
                if elem.GetDur() > 0 :
                    #print(step, elem.GetName(), elem.GetOp())
                    start_t = elem.GetStartTime()
                    end_t = start_t + elem.GetDur()
                    sys_cpu_util = self._cpu_util_data.GetCpuUtil(start_t, end_t)
                    total_area = elem.GetAreaBtwStartEnd(start_t, end_t)
                    op_util_ratio_with_intra = total_area / (intra_threads_num * (end_t - start_t))
                    op_util_ratio = total_area / (threads_num * (end_t - start_t))
                    sys_util_ratio = self._timeline_data.GetAllOpsAreaBtwStartEnd(step, start_t, end_t) / (threads_num * (end_t - start_t))

                    #print(sys_cpu_util, op_util_ratio, sys_util_ratio)
                    op_util_ratio_threshold = 0.2
                    sys_util_ratio_threshold = 0.4
                    sys_cpu_util_threshold = 0.5
                    if sys_cpu_util > sys_cpu_util_threshold and op_util_ratio > op_util_ratio_threshold and sys_util_ratio > sys_util_ratio_threshold :
                        comments = "Normal."
                    elif sys_cpu_util > sys_cpu_util_threshold and op_util_ratio < op_util_ratio_threshold and sys_util_ratio > sys_util_ratio_threshold :
                        comments = "Schedule analysis."
                    elif sys_cpu_util > sys_cpu_util_threshold and op_util_ratio > sys_util_ratio and op_util_ratio > op_util_ratio_threshold and sys_util_ratio < sys_util_ratio_threshold :
                        comments = "Exception."
                    elif sys_cpu_util > sys_cpu_util_threshold and op_util_ratio <= sys_util_ratio and op_util_ratio > op_util_ratio_threshold and sys_util_ratio < sys_util_ratio_threshold :
                        comments = "Schedule analysis or impacted by other workload."
                    elif sys_cpu_util > sys_cpu_util_threshold and op_util_ratio < op_util_ratio_threshold and sys_util_ratio < sys_util_ratio_threshold :
                        comments = "Impacted by other workload."
                    elif sys_cpu_util < sys_cpu_util_threshold and op_util_ratio > op_util_ratio_threshold and sys_util_ratio > sys_util_ratio_threshold :
                        comments = "Kernel analysis."
                    elif sys_cpu_util < sys_cpu_util_threshold and op_util_ratio < op_util_ratio_threshold and sys_util_ratio > sys_util_ratio_threshold :
                        comments = "Other OP instance analysis."
                    elif sys_cpu_util < sys_cpu_util_threshold and op_util_ratio > op_util_ratio_threshold and sys_util_ratio < sys_util_ratio_threshold :
                        comments = "Exception."
                    elif sys_cpu_util < sys_cpu_util_threshold and op_util_ratio < op_util_ratio_threshold and sys_util_ratio < sys_util_ratio_threshold :
                        comments = "Set proper thread number for the op instance, suggestion = logical core num."
                    name = elem.GetName()
                    try :
                        summary = summary_data[name]
                    except Exception as ex :
                        continue
                    list_info.append((step,  name, elem.GetOp(), summary.GetTotalArea(), total_area, sys_cpu_util, summary.GetOpUtilRatioWithIntra(), op_util_ratio_with_intra, op_util_ratio, sys_util_ratio, comments))
        d = pd.DataFrame(np.array(list_info))
        d.columns = ['step_id', 'node_name', 'op', 'summary_total_area', 'total_area', 'sys_cpu_util', 'summary_op_util_ratio_with_intra', 'op_util_ratio_with_intra', 'op_util_ratio', 'sys_util_ratio', 'comments']
        output_file = os.path.join(self._output_dir, 'schedule_analysis_result.csv')
        d.to_csv(output_file)

    # hw resource analysis function
    def _groundtruth_analysis(self) :
        list_info = []
        t = self._timeline_data
        gt_t = self._groundtruth_timeline_data
        # tmam groundtruth analysis
        t_tmam_d = t.GetTmamDistribution()
        if gt_t is None :
            for name in self._hotspot_name_list :
                try :
                    tmam_d = t_tmam_d[name]
                except Exception as ex :
                    continue

                raw_array = []
                retiring_raw = np.array(tmam_d.GetLevel1RetiringD().GetRawData())
                raw_array.append((retiring_raw,))

                bad_speculation_raw = np.array(tmam_d.GetLevel1BadSpeculationD().GetRawData())
                raw_array.append((bad_speculation_raw,))

                front_end_raw = np.array(tmam_d.GetLevel1FrontEndBoundD().GetRawData())
                raw_array.append((front_end_raw,))

                back_end_raw = np.array(tmam_d.GetLevel1BackEndBoundD().GetRawData())
                raw_array.append((back_end_raw,))

                branch_misperd_raw = np.array(tmam_d.GetLevel2BranchMisperdD().GetRawData())
                raw_array.append((branch_misperd_raw,))

                machine_clears_raw = np.array(tmam_d.GetLevel2MachineClearsD().GetRawData())
                raw_array.append((machine_clears_raw,))

                fetch_latency_raw = np.array(tmam_d.GetLevel2FetchLatencyD().GetRawData())
                raw_array.append((fetch_latency_raw,))

                fetch_bandwidth_raw = np.array(tmam_d.GetLevel2FetchBandwidthD().GetRawData())
                raw_array.append((fetch_bandwidth_raw,))

                memory_bound_raw = np.array(tmam_d.GetLevel2MemoryBoundD().GetRawData())
                raw_array.append((memory_bound_raw,))

                core_bound_raw = np.array(tmam_d.GetLevel2CoreBoundD().GetRawData())
                raw_array.append((core_bound_raw,))

                l1_bound_raw = np.array(tmam_d.GetLevel3L1BoundD().GetRawData())
                raw_array.append((l1_bound_raw,))

                l2_bound_raw = np.array(tmam_d.GetLevel3L2BoundD().GetRawData())
                raw_array.append((l2_bound_raw,))

                l3_bound_raw = np.array(tmam_d.GetLevel3L3BoundD().GetRawData())
                raw_array.append((l3_bound_raw,))

                dram_bound_raw = np.array(tmam_d.GetLevel3DramBoundD().GetRawData())
                raw_array.append((dram_bound_raw,))

                store_bound_raw = np.array(tmam_d.GetLevel3StoreBoundD().GetRawData())
                raw_array.append((store_bound_raw,))

                p50_array = []
                for i in raw_array :
                    p50 = round(np.percentile(i[0], 50), 2)
                    p50_array.append(p50)
                info = []
                info.append(name)
                info.extend(p50_array)
                list_info.append(info)

            d = pd.DataFrame(np.array(list_info))
            d.columns = ['node_name', 'level1_retiring_p50', 'level1_bad_speculation_p50', 'level1_front_end_bound_p50', 'level1_back_end_bound_p50', 'level2_branch_misperd_p50', 'level2_machine_clears_p50', 'level2_fetch_latency_p50', 'level2_fetch_bandwidth_p50', 'level2_memory_bound_p50', 'level2_core_bound_p50', 'level3_l1_bound_p50', 'level3_l2_bound_p50', 'level3_l3_bound_p50', 'level3_dram_bound_p50', 'level3_store_bound_p50']
            output_file = os.path.join(self._output_dir, 'tmam_analysis_result.csv')
            d.to_csv(output_file)
        else :
            gt_t_tmam_d = gt_t.GetTmamDistribution()
            for name in self._hotspot_name_list :
                try :
                    tmam_d = t_tmam_d[name]
                    gt_tmam_d = gt_t_tmam_d[name]
                except Exception as ex :
                    continue
                raw_array = []
                retiring_raw = np.array(tmam_d.GetLevel1RetiringD().GetRawData())
                gt_retiring_raw = np.array(gt_tmam_d.GetLevel1RetiringD().GetRawData())
                raw_array.append((retiring_raw, gt_retiring_raw))

                bad_speculation_raw = np.array(tmam_d.GetLevel1BadSpeculationD().GetRawData())
                gt_bad_speculation_raw = np.array(gt_tmam_d.GetLevel1BadSpeculationD().GetRawData())
                raw_array.append((bad_speculation_raw, gt_bad_speculation_raw))

                front_end_raw = np.array(tmam_d.GetLevel1FrontEndBoundD().GetRawData())
                gt_front_end_raw = np.array(gt_tmam_d.GetLevel1FrontEndBoundD().GetRawData())
                raw_array.append((front_end_raw, gt_front_end_raw))

                back_end_raw = np.array(tmam_d.GetLevel1BackEndBoundD().GetRawData())
                gt_back_end_raw = np.array(gt_tmam_d.GetLevel1BackEndBoundD().GetRawData())
                raw_array.append((back_end_raw, gt_back_end_raw))

                branch_misperd_raw = np.array(tmam_d.GetLevel2BranchMisperdD().GetRawData())
                gt_branch_misperd_raw = np.array(gt_tmam_d.GetLevel2BranchMisperdD().GetRawData())
                raw_array.append((branch_misperd_raw, gt_branch_misperd_raw))

                machine_clears_raw = np.array(tmam_d.GetLevel2MachineClearsD().GetRawData())
                gt_machine_clears_raw = np.array(gt_tmam_d.GetLevel2MachineClearsD().GetRawData())
                raw_array.append((machine_clears_raw, gt_machine_clears_raw))

                fetch_latency_raw = np.array(tmam_d.GetLevel2FetchLatencyD().GetRawData())
                gt_fetch_latency_raw = np.array(gt_tmam_d.GetLevel2FetchLatencyD().GetRawData())
                raw_array.append((fetch_latency_raw, gt_fetch_latency_raw))

                fetch_bandwidth_raw = np.array(tmam_d.GetLevel2FetchBandwidthD().GetRawData())
                gt_fetch_bandwidth_raw = np.array(gt_tmam_d.GetLevel2FetchBandwidthD().GetRawData())
                raw_array.append((fetch_bandwidth_raw, gt_fetch_bandwidth_raw))

                memory_bound_raw = np.array(tmam_d.GetLevel2MemoryBoundD().GetRawData())
                gt_memory_bound_raw = np.array(gt_tmam_d.GetLevel2MemoryBoundD().GetRawData())
                raw_array.append((memory_bound_raw, gt_memory_bound_raw))

                core_bound_raw = np.array(tmam_d.GetLevel2CoreBoundD().GetRawData())
                gt_core_bound_raw = np.array(gt_tmam_d.GetLevel2CoreBoundD().GetRawData())
                raw_array.append((core_bound_raw, gt_core_bound_raw))

                l1_bound_raw = np.array(tmam_d.GetLevel3L1BoundD().GetRawData())
                gt_l1_bound_raw = np.array(gt_tmam_d.GetLevel3L1BoundD().GetRawData())
                raw_array.append((l1_bound_raw, gt_l1_bound_raw))

                l2_bound_raw = np.array(tmam_d.GetLevel3L2BoundD().GetRawData())
                gt_l2_bound_raw = np.array(gt_tmam_d.GetLevel3L2BoundD().GetRawData())
                raw_array.append((l2_bound_raw, gt_l2_bound_raw))

                l3_bound_raw = np.array(tmam_d.GetLevel3L3BoundD().GetRawData())
                gt_l3_bound_raw = np.array(gt_tmam_d.GetLevel3L3BoundD().GetRawData())
                raw_array.append((l3_bound_raw, gt_l3_bound_raw))

                dram_bound_raw = np.array(tmam_d.GetLevel3DramBoundD().GetRawData())
                gt_dram_bound_raw = np.array(gt_tmam_d.GetLevel3DramBoundD().GetRawData())
                raw_array.append((dram_bound_raw, gt_dram_bound_raw))

                store_bound_raw = np.array(tmam_d.GetLevel3StoreBoundD().GetRawData())
                gt_store_bound_raw = np.array(gt_tmam_d.GetLevel3StoreBoundD().GetRawData())
                raw_array.append((store_bound_raw, gt_store_bound_raw))

                p50_diff_array = []
                for i in raw_array :
                    p50 = np.percentile(i[0], 50)
                    gt_p50 = np.percentile(i[1], 50)
                    p50_diff_array.append((p50 - gt_p50)/gt_p50)
                    p50_diff_array.append((p50 - gt_p50))
                info = []
                info.append(name)
                info.extend(p50_diff_array)
                list_info.append(info)
            d = pd.DataFrame(np.array(list_info))
            d.columns = ['node_name', 'level1_retiring_p50_diff(relative)', 'level1_retiring_p50_diff(abs)', 'level1_bad_speculation_p50_diff(relative)', 'level1_bad_speculation_p50_diff(abs)', 'level1_front_end_bound_p50_diff(relative)', 'level1_front_end_bound_p50_diff(abs)', 'level1_back_end_bound_p50_diff(relative)', 'level1_back_end_bound_p50_diff(abs)', 'level2_branch_misperd_p50_diff(relative)', 'level2_branch_misperd_p50_diff(abs)', 'level2_machine_clears_p50_diff(relative)', 'level2_machine_clears_p50_diff(abs)', 'level2_fetch_latency_p50_diff(relative)', 'level2_fetch_latency_p50_diff(abs)', 'level2_fetch_bandwidth_p50_diff(relative)', 'level2_fetch_bandwidth_p50_diff(abs)', 'level2_memory_bound_p50_diff(relative)', 'level2_memory_bound_p50_diff(abs)', 'level2_core_bound_p50_diff(relative)', 'level2_core_bound_p50_diff(abs)', 'level3_l1_bound_p50_diff(relative)', 'level3_l1_bound_p50_diff(abs)', 'level3_l2_bound_p50_diff(relative)', 'level3_l2_bound_p50_diff(abs)', 'level3_l3_bound_p50_diff(relative)', 'level3_l3_bound_p50_diff(abs)', 'level3_dram_bound_p50_diff(relative)', 'level3_dram_bound_p50_diff(abs)', 'level3_store_bound_p50_diff(relative)', 'level3_store_bound_p50_diff(abs)']
            output_file = os.path.join(self._output_dir, 'groundtruth_tmam_analysis_result.csv')
            d.to_csv(output_file)


# the class supports auto analysis function(QPS strategy) using timeline data as input
class AutoAnalysisQPS(AutoAnalysis) :

    #init the instance with input data
    def __init__(self, timeline_data, groundtruth_timeline_data, cpu_util_data, output_dir) :
        super(AutoAnalysisQPS, self).__init__('QPS', timeline_data, groundtruth_timeline_data, cpu_util_data, output_dir)
        self.__qps_info = {}

    # generate the hotspot list with order, for QPS strategy
    def _generate_hotspot_list(self) :
        if self._groundtruth_timeline_data is None :
            step_id_list = self._timeline_data.GetStepIdList()
            op_name_list = self._timeline_data.GetOpNameList()
            self._hotspot_name_list = self.GenerateHotspotNameList(step_id_list, op_name_list)
        else :
            self._hotspot_name_list = self.GenerateHotspotNameListOnlineVsOffline()

    def GetTotalInfo(self, step_id_list, op_name_list, data) :
        ret_data = {}
        for step in step_id_list :
            for name in op_name_list :
                for elem in data[step] :
                    if (elem.GetName()) == name and (elem.GetDur() > 0) :
                        start_t = elem.GetStartTime()
                        end_t = start_t + elem.GetDur()
                        op = elem.GetOp()
                        total_area = elem.GetAreaBtwStartEnd(start_t, end_t)
                        op_util_ratio_with_intra = 0
                        if name not in ret_data :
                            ret_data[name] = []
                            ret_data[name].append((op, total_area, op_util_ratio_with_intra))
                        else :
                            ret_data[name].append((op, total_area, op_util_ratio_with_intra))
        return ret_data

    def GenerateHotspotNameListOnlineVsOffline(self) :
        online_data = self._timeline_data.GetData()
        offline_data = self._groundtruth_timeline_data.GetData()

        online_step_id_list = self._timeline_data.GetStepIdList()
        online_op_name_list = self._timeline_data.GetOpNameList()

        offline_step_id_list = self._groundtruth_timeline_data.GetStepIdList()
        online_info = self.GetTotalInfo(online_step_id_list, online_op_name_list, online_data)
        offline_info = self.GetTotalInfo(offline_step_id_list, online_op_name_list, offline_data)

        data_info = []
        for k in online_info.keys() :
            online_v = online_info[k]
            total_area = 0.0
            size = len(online_v)
            op = None
            if size > 0 :
                for e in online_v :
                    op = e[0]
                    total_area = total_area + e[1]
            try :
                offline_v = offline_info[k]
            except Exception as ex :
                continue
            size = len(offline_v)
            if size > 0 :
                for e in offline_v :
                    op = e[0]
                    total_area = total_area - e[1]
            summary = ProfileDataSummary(k, op)
            summary.SetTotalData(total_area)
            data_info.append(summary)

        data_info.sort(key = lambda t: t.GetTotalArea(), reverse=True)

        ret = []
        for e in data_info :
            name = e.GetName()
            ret.append(name)
            self.__qps_info[name] = e
        return ret

    def GenerateHotspotNameList(self, step_id_list, op_name_list) :
        data = self._timeline_data.GetData()
        data_info = []
        tmp_data =  self.GetTotalInfo(step_id_list, op_name_list, data)

        for k in tmp_data.keys() :
            v = tmp_data[k]
            total_area = 0.0
            size = len(v)
            op = None
            if size > 0 :
                for e in v :
                    op = e[0]
                    total_area = total_area + e[1]
            summary = ProfileDataSummary(k, op)
            summary.SetTotalData(total_area)
            data_info.append(summary)

        data_info.sort(key = lambda t: t.GetTotalArea(), reverse=True)

        ret = []
        for e in data_info :
            name = e.GetName()
            ret.append(name)
            self.__qps_info[name] = e
        return ret

    # hw resource analysis function
    def _groundtruth_analysis(self) :
        super(AutoAnalysisQPS, self)._groundtruth_analysis()
        list_info = []
        t = self._timeline_data
        gt_t = self._groundtruth_timeline_data
        # qps groundtruth analysis
        t_qps_area_d = t.GetQpsAreaDistribution()
        if gt_t is None :
            all_total_area = 0.0
            filter_info = []
            for name in self._hotspot_name_list :
                all_total_area += self.__qps_info[name].GetTotalArea()

            for name in self._hotspot_name_list :
                op = self.__qps_info[name].GetOp()
                total_area = self.__qps_info[name].GetTotalArea()
                qps_area_d = t_qps_area_d[name]
                ratio = round(total_area/all_total_area, 2)
                if ratio >= 0.02 :
                    list_info.append((name, op, round(ratio * 100, 2)))
                    filter_info.append(name)
            d = pd.DataFrame(np.array(list_info))
            d.columns = ['node_name', 'op', 'area(sort)']
            output_file = os.path.join(self._output_dir, 'qps_analysis_result.csv')
            d.to_csv(output_file)
            super(AutoAnalysisQPS, self)._generate_suggestions(filter_info, 'qps_opt_suggestion.csv')
        else :
            gt_t_qps_area_d = gt_t.GetQpsAreaDistribution()

            for name in self._hotspot_name_list :
                op = self.__qps_info[name].GetOp()
                total_area = self.__qps_info[name].GetTotalArea()
                qps_area_d = t_qps_area_d[name]
                try :
                    gt_qps_area_d = gt_t_qps_area_d[name]
                except Exception as ex :
                    continue
                qps_area_js = JsDivergence(qps_area_d, gt_qps_area_d)
                qps_area_raw = np.array(qps_area_d.GetRawData())
                gt_qps_area_raw = np.array(gt_qps_area_d.GetRawData())
                p50 = np.percentile(qps_area_raw, 50)
                gt_p50 = np.percentile(gt_qps_area_raw, 50)
                p75 = np.percentile(qps_area_raw, 75)
                gt_p75 = np.percentile(gt_qps_area_raw, 75)
                p90 = np.percentile(qps_area_raw, 90)
                gt_p90 = np.percentile(gt_qps_area_raw, 90)
                p95 = np.percentile(qps_area_raw, 95)
                gt_p95 = np.percentile(gt_qps_area_raw, 95)
                list_info.append((name, op, total_area, qps_area_js.Compute(), ((p50 - gt_p50)/gt_p50), ((p75 - gt_p75)/gt_p75), ((p90 - gt_p90)/gt_p90), ((p95 - gt_p95)/gt_p95)))

            d = pd.DataFrame(np.array(list_info))
            d.columns = ['node_name', 'op', 'area(sort)', 'qps_area_js', 'p50 diff', 'p75 diff', 'p90 diff', 'p95 diff']
            output_file = os.path.join(self._output_dir, 'groundtruth_qps_analysis_result.csv')
            d.to_csv(output_file)

# the class supports auto analysis function(Latency strategy) using timeline data as input
class AutoAnalysisLatency(AutoAnalysis) :

    #init the instance with input data
    def __init__(self, timeline_data, groundtruth_timeline_data, cpu_util_data, output_dir) :
        super(AutoAnalysisLatency, self).__init__('Latency', timeline_data, groundtruth_timeline_data, cpu_util_data, output_dir)
        self.__latency_info = {}

    # generate the hotspot list with order, for Latency strategy
    def _generate_hotspot_list(self) :
        if self._groundtruth_timeline_data is None :
            step_id_list = self._timeline_data.GetStepIdList()
            name_info = self._timeline_data.FindCriticalPath()
            self._hotspot_name_list = self.GenerateHotspotNameList(step_id_list, name_info)
        else :
            self._hotspot_name_list = self.GenerateHotspotNameListOnlineVsOffline()

    def GetTotalInfo(self, step_id_list, op_name_list, data) :
        ret_data = {}
        for step in step_id_list :
            for name in op_name_list[step] :
                for elem in data[step] :
                    if (elem.GetName()) == name and (elem.GetDur() > 0) :
                        op = elem.GetOp()
                        total_area = elem.GetDur()
                        op_util_ratio_with_intra = 0
                        if name not in ret_data :
                            ret_data[name] = []
                            ret_data[name].append((op, total_area, op_util_ratio_with_intra))
                        else :
                            ret_data[name].append((op, total_area, op_util_ratio_with_intra))
        return ret_data

    def get_total_info_with_list(self, step_id_list, op_name_list, data) :
        ret_data = {}
        for step in step_id_list :
            for name in op_name_list :
                for elem in data[step] :
                    if (elem.GetName()) == name and (elem.GetDur() > 0) :
                        op = elem.GetOp()
                        total_area = elem.GetDur()
                        op_util_ratio_with_intra = 0
                        if name not in ret_data :
                            ret_data[name] = []
                            ret_data[name].append((op, total_area, op_util_ratio_with_intra))
                        else :
                            ret_data[name].append((op, total_area, op_util_ratio_with_intra))
        return ret_data

    def GenerateHotspotNameListOnlineVsOffline(self) :
        online_data = self._timeline_data.GetData()
        offline_data = self._groundtruth_timeline_data.GetData()

        online_step_id_list = self._timeline_data.GetStepIdList()
        online_op_name_list = self._timeline_data.FindCriticalPath()
        op_name_list = []
        for k in online_op_name_list.keys() :
            op_name_list.extend(online_op_name_list[k])
        op_name_list = list(set(op_name_list))

        offline_step_id_list = self._groundtruth_timeline_data.GetStepIdList()
        online_info = self.get_total_info_with_list(online_step_id_list, op_name_list, online_data)
        offline_info = self.get_total_info_with_list(offline_step_id_list, op_name_list, offline_data)

        data_info = []
        for k in online_info.keys() :
            online_v = online_info[k]
            total_area = 0.0
            size = len(online_v)
            op = None
            if size > 0 :
                for e in online_v :
                    op = e[0]
                    total_area = total_area + e[1]
            try :
                offline_v = offline_info[k]
            except Exception as ex :
                continue
            size = len(offline_v)
            if size > 0 :
                for e in offline_v :
                    op = e[0]
                    total_area = total_area - e[1]
            summary = ProfileDataSummary(k, op)
            summary.SetTotalData(total_area)
            data_info.append(summary)

        data_info.sort(key = lambda t: t.GetTotalArea(), reverse=True)

        ret = []
        for e in data_info :
            name = e.GetName()
            ret.append(name)
            self.__latency_info[name] = e
        return ret

    def GenerateHotspotNameList(self, step_id_list, op_name_list) :
        data = self._timeline_data.GetData()
        data_info = []
        tmp_data =  self.GetTotalInfo(step_id_list, op_name_list, data)

        for k in tmp_data.keys() :
            v = tmp_data[k]
            total_area = 0.0
            size = len(v)
            op = None
            if size > 0 :
                for e in v :
                    op = e[0]
                    total_area = total_area + e[1]
            summary = ProfileDataSummary(k, op)
            summary.SetTotalData(total_area)
            data_info.append(summary)

        data_info.sort(key = lambda t: t.GetTotalArea(), reverse=True)
        ret = []
        for e in data_info :
            if e.GetOp() != "_MklReshape" or e.GetOp() != "Reshape" :
                ret.append(e.GetName())
                self.__latency_info[e.GetName()] = e
        return ret

    # hw resource analysis function
    def _groundtruth_analysis(self) :
        super(AutoAnalysisLatency, self)._groundtruth_analysis()
        list_info = []
        detail_list_info = []
        t = self._timeline_data
        gt_t = self._groundtruth_timeline_data
        # latency groundtruth analysis
        t_latency_d = t.GetLatencyDistribution()
        if gt_t is None :
            filter_info = []
            all_total_latency = 0.0
            for name in self._hotspot_name_list :
                all_total_latency += self.__latency_info[name].GetTotalArea()
            for name in self._hotspot_name_list :
                op = self.__latency_info[name].GetOp()
                total_latency = self.__latency_info[name].GetTotalArea()
                ratio = round(total_latency/all_total_latency, 2)
                if ratio >= 0.02 :
                    list_info.append((name, op, round(ratio * 100, 2)))
                    filter_info.append(name)
            d = pd.DataFrame(np.array(list_info))
            d.columns = ['node_name', 'op', 'latency(sort)']
            output_file = os.path.join(self._output_dir, 'latency_analysis_result.csv')
            d.to_csv(output_file)
            super(AutoAnalysisLatency, self)._generate_suggestions(filter_info, 'latency_opt_suggestion.csv')
        else :
            gt_t_latency_d = gt_t.GetLatencyDistribution()

            for name in self._hotspot_name_list :
                op = self.__latency_info[name].GetOp()
                total_latency = self.__latency_info[name].GetTotalArea()
                latency_d = t_latency_d[name]
                try :
                    gt_latency_d = gt_t_latency_d[name]
                except Exception as ex :
                    continue
                latency_js = JsDivergence(latency_d, gt_latency_d)
                latency_raw = np.array(latency_d.GetRawData())
                gt_latency_raw = np.array(gt_latency_d.GetRawData())

                p50 = np.percentile(latency_raw, 50)
                gt_p50 = np.percentile(gt_latency_raw, 50)
                p75 = np.percentile(latency_raw, 75)
                gt_p75 = np.percentile(gt_latency_raw, 75)
                p90 = np.percentile(latency_raw, 90)
                gt_p90 = np.percentile(gt_latency_raw, 90)
                p95 = np.percentile(latency_raw, 95)
                gt_p95 = np.percentile(gt_latency_raw, 95)
                list_info.append((name, op, total_latency, latency_js.Compute(), ((p50 - gt_p50)/gt_p50), ((p75 - gt_p75)/gt_p75), ((p90 - gt_p90)/gt_p90), ((p95 - gt_p95)/gt_p95)))

            d = pd.DataFrame(np.array(list_info))
            d.columns = ['node_name', 'op', 'latency(sort)', 'latency_js', 'p50 diff', 'p75 diff', 'p90 diff', 'p95 diff']
            output_file = os.path.join(self._output_dir, 'groundtruth_latency_analysis_result.csv')
            d.to_csv(output_file)

def PathCheck(list_of_path):
    # proj_root = "/".join(os.path.abspath(os.path.curdir).split("/")[0:-1])
    proj_root = os.path.abspath(os.path.curdir)

    for path in list_of_path:
        if not os.path.exists(path):
            print(f"Failed. Path {path} does not exist.")
            return False

    for path in list_of_path:
        if os.path.islink(path):
            print(f"Failed. Path {path} is a symlink.")
            return False

        if path is None or not proj_root in os.path.abspath(path) or ".." in os.path.abspath(path):
            print(f"Failed. Path {path} is None or is out side {proj_root}. All path should inside the project root and can not contain ..")
            return False
    return True

if __name__ == "__main__" :
    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument("-g", type=str, default=None, help="Path to groundturth timeline, generated by postprocess for groundtruth case")
    arg_parser.add_argument("-i", type=str, required=True, default=None, help="Path to runtime timeline, generated by postprocess")
    arg_parser.add_argument("-t", type=str, required=True, default="QPS", help="target of auto anlysis, supported QPS or LATENCY")
    arg_parser.add_argument("-o", type=str, required=True, default=None, help="Path to store output file")
    args = arg_parser.parse_args()

    path_valid = True
    if args.g is not None :
        path_valid &= PathCheck([args.i, args.g, args.o])
    else :
        path_valid &= PathCheck([args.i, args.o])

    if not path_valid :
        print("path is invalid.");
        sys.exit(-1)

    output_dir = args.o
    d= TimelineData(args.i)
    #d = TimelineData('test_timelines/new_graph_2inst_6usr.json')
    gt_d = None
    if args.g is not None :
        gt_d = TimelineData(args.g)
    #gt_d = TimelineData('test_timelines/new_graph_1inst_8usr.json')
    cpu_d = CpuUtilData(None)

    if args.t == "QPS" :
        analysis = AutoAnalysisQPS(d, gt_d, cpu_d, output_dir)
    else :
        analysis = AutoAnalysisLatency(d, gt_d, cpu_d, output_dir)
    analysis.Analysis()
