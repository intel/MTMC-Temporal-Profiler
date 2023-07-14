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
import time

import numpy as np
import os
import json
import _pickle as pickle

from tensorflow_core.python.client import timeline
from tensorflow.core.framework.step_stats_pb2 import StepStats
from multiprocessing import Pool, freeze_support, cpu_count

import argparse
from perfmon_parser import PerfmonParser
import tma_cal as tc
from tma_cal import MIN, MAX

def split(a, n):
    k, m = divmod(len(a), n)
    return [a[i*k+min(i, m):(i+1)*k+min(i+1, m)] for i in range(n)]

cores = min(os.cpu_count(), 48)

# class SingleLog:
#     def __init__(self, line):
#         # print(line)
#         self.tid = int(line[0])
#         self.pthread_id = int(line[1])
#         self.time_begin = int(line[2])
#         self.time_end = int(line[3])
#         self.parent_tid = int(line[4])
#         self.parent_pthread_id = int(line[5])
#         self.parent_sched_time = int(line[6])
#         self.int_prefix = int(line[7])
#         self.topdown_metrics = [int(i) for i in line[8].split('_')] # a_slot, a_metric, b_slot, b_metric
#         if len(self.topdown_metrics) != 6:
#             print("Empty topdown metric")
#         self.b_event_num, self.b_core, self.b_prefix = [int(i) for i in line[9].split('_')]
#         self.b_events = [np.float(i) for i in line[10].split('_')]
#         self.e_event_num, self.e_core, self.e_prefix = [int(i) for i in line[11].split('_')]
#         self.e_events = [np.float(i) for i in line[12].split('_')]
#         self.schedular = None

kSingleLogLen = 14

class SingleLog:
    def __init__(self, line):
        self.tid = int(line[0])
        self.pthread_id = int(line[1])
        self.time_begin = int(line[2])
        self.time_end = int(line[3])
        self.parent_tid = int(line[4])
        self.parent_pthread_id = int(line[5])
        self.parent_sched_time = int(line[6])
        self.topdown_metrics = [int(i) for i in line[7].split('_')] # a_slot, a_metric, b_slot, b_metric
        if len(self.topdown_metrics) != 6:
            print("Empty topdown metric")
        self.b_event_num, self.b_core, self.b_prefix = [int(i) for i in line[8].split('_')]
        self.b_events = [int(i) for i in line[9].split('_')]
        self.e_event_num, self.e_core, self.e_prefix = [int(i) for i in line[10].split('_')]
        self.e_events = [int(i) for i in line[11].split('_')]
        self.int_prefix = int(line[12])
        self.prefix = str(line[13])
        self.cfg_idx = int(line[14])
        self.cnst = [float(i) for i in line[15].split('_') if i.isdigit()]
        self.schedular = None

# For Tensorflow 1.15.0 profiler
class TimelineWorker:
    def __init__(self, output_folder=None):
        self.output_folder = output_folder
        self.step_state = None

    def ReadStepState(self, step_state_addr):
        self.step_stats = StepStats()
        with open(step_state_addr, "rb") as f:
            self.step_stats.MergeFromString(f.read())
        return self.step_stats

    def StepState2Timeline(self, step_state_addr, output_path=None):
        step_stats = StepStats()
        with open(step_state_addr, "rb") as f:
            step_stats.ParseFromString(f.read())
        x = timeline.Timeline(step_stats).generate_chrome_trace_format()

        output_full_path = None
        if output_path is not None:
            output_full_path = output_path
        elif self.output_folder is not None:
            output_full_path = os.path.join(self.output_folder, "default_timeline.json")

        if output_full_path is not None:
            with open(output_full_path, 'w') as f:
                f.write(x)
        return x

    def GetMinMaxTime(self, step_state_addr):
        step_stats = StepStats()
        with open(step_state_addr, "rb") as f:
            step_stats.ParseFromString(f.read())
        perthread = step_stats.dev_stats[0]
        nodes = perthread.node_stats

        starttm = []
        endtm = []
        for idx, node in enumerate(nodes):
            starttm.append(node.all_start_micros)
            endtm.append(node.all_start_micros + node.all_end_rel_micros)

        mintime = np.min(np.array(starttm))
        maxtime = np.max(np.array(endtm))

        return mintime, maxtime

    def ReadTimelineJson(self, timeline_addr):
        with open(timeline_addr) as f:
            loaded = json.load(f)
        return loaded

class TimelineMapper:
    """
    sorted_node_state: Node state from tf step_state that sorted in an increasing order. List[(int, node_state)]
    inter_op_threads: Inter op threads name. (From tensorflow step_state). dict{threadid : "name"}
    raw_logs: Raw mtmc logs. List[SingleLog]
    """
    def __init__(self, sorted_node_state, inter_op_pthreads):
        self.sorted_node_state = sorted_node_state
        self.inter_op_pthreads = inter_op_pthreads

        self.kRight = 0
        self.kLeft = 1

        self.kNotSchedularInterOp = -2
        self.kNotFound = -3

        self.valid = True
        if self.sorted_node_state is None or self.inter_op_pthreads is None:
            self.valid = False
            print("No sorted node state found, timeline mapper will not function correctly")

    def CheckWorker(self, raw_logs):
        last_place = 0
        mapped_logs = []
        recheck_logs = []
        failed_logs = []
        for single_log in raw_logs:
            # Schedular is not in the inter op thread pool, just return the code
            if not single_log.parent_pthread_id in self.inter_op_pthreads.keys():
                keys = self.inter_op_pthreads.keys()
                recheck_logs.append(single_log)
                # TODO: Inter op threads SHOULD USE GET KTID
                continue

            status, idx = self.Check(single_log, self.kRight, last_place)
            if status == idx:
                # print(f"Check right found! idx:{idx}")
                last_place = idx
                single_log.schedular = self.sorted_node_state[idx][1]
                mapped_logs.append(single_log)
                continue

            status, idx = self.Check(single_log, self.kLeft, last_place)
            if status == idx:
                # print(f"Check left found! idx:{idx}")
                last_place = idx
                single_log.schedular = self.sorted_node_state[idx][1]
                mapped_logs.append(single_log)
                continue

            failed_logs.append(single_log)
        return mapped_logs, recheck_logs, failed_logs

    def RecheckWorker(self, intput_list):
        recheck_logs, mapped_logs = intput_list
        recheck_temp = []
        temp_mapped_log = []
        last_place = 0
        for single_recheck_log in recheck_logs:
            status, idx = self.ReCheck(single_recheck_log, self.kRight, last_place, mapped_logs)
            if status == idx and mapped_logs[idx].schedular is not None:
                # print(f"Recheck right found! idx:{idx}")
                last_place = idx
                single_recheck_log.schedular = mapped_logs[idx].schedular
                temp_mapped_log.append(single_recheck_log)
                continue

            status, idx = self.ReCheck(single_recheck_log, self.kLeft, last_place, mapped_logs)
            if status == idx and mapped_logs[idx].schedular is not None:
                # print(f"Recheck left found! idx:{idx}")
                last_place = idx
                single_recheck_log.schedular = mapped_logs[idx].schedular
                temp_mapped_log.append(single_recheck_log)
                continue

            recheck_temp.append(single_recheck_log)
        return recheck_temp, temp_mapped_log

    def Search(self, raw_logs, pool):

        if not self.valid:
            print("Search failed due to the timeline mapper is not valid")
            return None

        mapped_logs = []
        recheck_logs = []
        failed_logs = []

        raw_log_splits = split(raw_logs, cores)

        ret = pool.map(self.CheckWorker, raw_log_splits)

        for sig_ret in ret:
            mapped_logs += sig_ret[0]
            recheck_logs += sig_ret[1]
            failed_logs += sig_ret[2]

        last_mapped_logs_len = len(mapped_logs)
        itr = 0
        while len(recheck_logs) > 0:
            recheck_temp = []
            temp_mapped_log = []

            recheck_logs_splits = split(recheck_logs, cores)
            recheck_args = [(sig_split, mapped_logs) for sig_split in recheck_logs_splits]

            ret = pool.map(self.RecheckWorker, recheck_args)

            for sig_ret in ret:
                recheck_temp += sig_ret[0]
                temp_mapped_log += sig_ret[1]

            mapped_logs += temp_mapped_log
            mapped_logs = sorted(mapped_logs, key=lambda x:x.time_begin)
            recheck_logs = recheck_temp
            itr += 1

            if last_mapped_logs_len == len(mapped_logs):
                print(f"Recheck exit due to no more node can be mapped. {len(recheck_logs)}")
                break
            else:
                print(f"Next recheck iteration {itr}")
                last_mapped_logs_len = len(mapped_logs)

        print("Search done")
        return mapped_logs, recheck_logs, failed_logs

    def SearchSingleThread(self, raw_logs):

        if not self.valid:
            print("Search failed due to the timeline mapper is not valid")
            return None

        # print(f"Inter ops pthreads\n{self.inter_op_pthreads}")
        last_place = 0
        mapped_logs = []
        recheck_logs = []
        failed_logs = []
        interop_log = []

        for single_log in raw_logs:

            if single_log.prefix.split("~")[0] == "INTEROP":
                interop_log.append(single_log)
                continue

            # Schedular is not in the inter op thread pool, just return the code
            if not single_log.parent_pthread_id in self.inter_op_pthreads.keys():
                keys = self.inter_op_pthreads.keys()
                recheck_logs.append(single_log)
                #TODO: Inter op threads SHOULD USE GET KTID
                continue

            status, idx = self.Check(single_log, self.kRight, last_place)
            if status == idx:
                # print(f"Check right found! idx:{idx}")
                last_place = idx
                single_log.schedular = self.sorted_node_state[idx][1]
                mapped_logs.append(single_log)
                continue

            status, idx = self.Check(single_log, self.kLeft, last_place)
            if status == idx:
                # print(f"Check left found! idx:{idx}")
                last_place = idx
                single_log.schedular = self.sorted_node_state[idx][1]
                mapped_logs.append(single_log)
                continue

            failed_logs.append(single_log)
            # print(f"Found nothing! Error!")

        last_place = 0
        last_mapped_logs_len = len(mapped_logs)
        itr = 0
        while len(recheck_logs) > 0:
            recheck_temp = []
            temp_mapped_log = []
            for single_recheck_log in recheck_logs:
                status, idx = self.ReCheck(single_recheck_log, self.kRight, last_place, mapped_logs)
                if status == idx and mapped_logs[idx].schedular is not None:
                    # print(f"Recheck right found! idx:{idx}")
                    last_place = idx
                    single_recheck_log.schedular = mapped_logs[idx].schedular
                    temp_mapped_log.append(single_recheck_log)
                    continue

                status, idx = self.ReCheck(single_recheck_log, self.kLeft, last_place, mapped_logs)
                if status == idx and mapped_logs[idx].schedular is not None:
                    # print(f"Recheck left found! idx:{idx}")
                    last_place = idx
                    single_recheck_log.schedular = mapped_logs[idx].schedular
                    temp_mapped_log.append(single_recheck_log)
                    continue

                recheck_temp.append(single_recheck_log)
                # print("Recheck not found error")

            mapped_logs += temp_mapped_log
            mapped_logs = sorted(mapped_logs, key=lambda x:x.time_begin)
            recheck_logs = recheck_temp
            itr += 1

            if last_mapped_logs_len == len(mapped_logs):
                print(f"Recheck exit due to no more node can be mapped. {len(recheck_logs)}")
                break
            else:
                print(f"Next recheck iteration {itr}")
                last_mapped_logs_len = len(mapped_logs)

        print("Search done")
        return mapped_logs, recheck_logs, failed_logs, interop_log

    def Check(self, single_log, direction, last_place): # direction: 0 for left, 1 for right

        if not self.valid:
            print("Check failed due to the timeline mapper is not valid")
            return None

        # Schedular is not in the inter op thread pool, just return the code
        if not single_log.parent_pthread_id in self.inter_op_pthreads.keys():
            return self.kNotSchedularInterOp, last_place

        # Use last place to reduce calculation time
        idx = last_place + 0
        if idx < 0 or idx >= len(self.sorted_node_state):
            idx = 0

        while 0 <= idx < len(self.sorted_node_state):
            inter_state = self.sorted_node_state[idx][1] # Sorted_node_state is a tuple of (time_start_ns, node_state)
            if direction == self.kLeft: # Check left
                if single_log.parent_pthread_id == inter_state.thread_id:
                    if inter_state.all_start_micros <= single_log.parent_sched_time <= inter_state.all_start_micros + inter_state.all_end_rel_micros:
                        # Found
                        return idx, idx
                idx -= 1
            elif direction == self.kRight: # Check right
                # Loop to the place where parent starting time is bigger than child starting, return not found
                if inter_state.all_start_micros > single_log.parent_sched_time:
                    return self.kNotFound, idx

                if single_log.parent_pthread_id == inter_state.thread_id:
                    if inter_state.all_start_micros + inter_state.all_end_rel_micros >= single_log.parent_sched_time:
                        # Found
                        return idx, idx
                idx += 1
        return self.kNotFound, last_place

    def ReCheck(self, single_log, direction, last_place, mapped_logs):

        if not self.valid:
            print("Search failed due to the timeline mapper is not valid")
            return None

        idx = last_place + 0
        if idx < 0 or idx >= len(mapped_logs):
            idx = 0

        while 0 <= idx < len(mapped_logs):
            mapped_single_log = mapped_logs[idx]
            # Right
            if direction == self.kLeft:
                if mapped_single_log.time_begin <= single_log.parent_sched_time <= mapped_single_log.time_end:
                    if single_log.parent_pthread_id == mapped_single_log.pthread_id:
                        # Found
                        return idx, idx
                idx -= 1

            # Left
            elif direction == self.kRight:
                if mapped_single_log.time_begin > single_log.parent_sched_time:
                    return self.kNotFound, idx

                if single_log.parent_pthread_id == mapped_single_log.pthread_id:
                    if mapped_single_log.time_end >= single_log.parent_sched_time:
                        # Found
                        return idx, idx
                idx += 1

        return self.kNotFound, last_place

    # Support new MTMC profiler to generate without timeline
    def GeneralizeSingleThread(self, raw_logs):
        intraop_log = []
        interop_log = []

        for single_log in raw_logs:
            if single_log.prefix.split("~")[0] == "INTEROP":
                interop_log.append(single_log)
            else:
                intraop_log.append(single_log)

        return intraop_log, interop_log

    def GeneralizeWithIntPrefixRawLogs(self, pool, int_prefix_raw_logs):
        intraop_logs = []
        interop_logs = []

        ret = pool.map(self.GeneralizeSingleThread, int_prefix_raw_logs.values())
        for sig_ret in ret:
            intraop_logs += sig_ret[0]
            interop_logs += sig_ret[1]

        return interop_logs, intraop_logs

    def SearchWithIntPrefixRawLogs(self, pool, int_prefix_raw_logs):
        mapped_logs = []
        recheck_logs = []
        failed_logs = []
        interop_logs = []

        # Single-thread
        # for raw_logs in int_prefix_raw_logs.values():
        #     itr_mapped_logs, itr_recheck_logs, itr_failed_logs = self.SearchSingleThread(raw_logs)
        #     mapped_logs += itr_mapped_logs
        #     recheck_logs += itr_recheck_logs
        #     failed_logs += itr_failed_logs

        # Multi-thread
        ret = pool.map(self.SearchSingleThread, int_prefix_raw_logs.values())
        for sig_ret in ret:
            mapped_logs += sig_ret[0]
            recheck_logs += sig_ret[1]
            failed_logs += sig_ret[2]
            interop_logs += sig_ret[3]

        return mapped_logs, recheck_logs, failed_logs, interop_logs

class PerfMapper:

    def __init__(self, thp_info_folder=None):
        self.clk_source = "MON"
        self.clk_diff = 0
        self.thp_info_folder = None
        if thp_info_folder is not None:
            self.thp_info_folder = thp_info_folder

        self.target_ths = []
        if self.thp_info_folder is not None:
            print(f"Read threadpool information from: {self.thp_info_folder}")
            files = os.listdir(self.thp_info_folder)
            for thp in files:
                with open(os.path.join(self.thp_info_folder, thp), 'r') as f:
                    for line in f.readlines():
                        self.target_ths.append(line.split('\n')[0].split(',')[1])

        self.abs_path = os.path.dirname(os.path.abspath(__file__))

    def CalClock(self, path_to_clkcal):
        if not os.path.exists(path_to_clkcal):
            raise Exception("Calculate clock different failed due to no clock log found.")

        diff_arr = []
        with open(path_to_clkcal, 'r') as f:
            lines = f.readlines()
            for line in lines:
                s1, s2 = line.split(',')
                diff_arr.append(abs(int(s1)-int(s2)))
        print(diff_arr)
        print(diff_arr[1]-diff_arr[0])
        self.clk_diff = np.average(diff_arr)
        return self.clk_diff

    def CtxSwitchDataMapping(self, path_to_txt_data):
        # The output is a dictionary where key is thread_ids and values are a two lists, first one is the time
        output_core = {}
        output_thread = {}

        with open(path_to_txt_data, 'r') as f:
            lines = f.readlines()
            for line in lines:
                segments = line.split(' ')
                # print(segments)
                data = [None, None, None, None, None, None] # 0 - core, 1 - time, 2 - prev tid , 3 - prev name, 4 - next tid, 5 - next name
                for single_seg in reversed(segments):
                    if 'next_pid' in single_seg:
                        data[4] = single_seg.split('next_pid=')[-1]
                    elif 'prev_pid' in single_seg:
                        data[2] = single_seg.split('prev_pid=')[-1]
                    elif '.' in single_seg:
                        data[1] = float(single_seg.split(':')[0]) * 1000 * 1000 *1000
                    elif '[' in single_seg:
                        data[0] = int(single_seg.split(']')[0].split('[')[-1])
                if data[0] in output_core.keys():
                    output_core[data[0]].append(data)
                else:
                    output_core[data[0]] = [data]

                if data[2] in output_thread.keys():
                    output_thread[data[2]].append(['e', data])
                else:
                    output_thread[data[2]] = [['e', data]]

                if data[4] in output_thread.keys():
                    output_thread[data[4]].append(['b', data])
                else:
                    output_thread[data[4]] = [['b', data]]
        return output_core, output_thread

    def ParsePerfData(self, path_to_perf_data):
        # TODO: HARDCODE
        perf_dest = os.path.join(os.path.dirname(path_to_perf_data),'perf.data.txt')
        ret = os.system(f"perf script -i {path_to_perf_data} --ns -F cpu,trace,time,comm > {perf_dest}")
        # sp.run(f"perf script -i {path_to_perf_data} --ns -F cpu,trace,time,comm > {perf_dest}",shell=True)
        if ret :
            raise Exception(f"Perf command error! {ret}")

        return self.CtxSwitchDataMapping(perf_dest)

    def LoadPerfData(self, path_to_perf_data_bin):
        with open(path_to_perf_data_bin, 'rb') as f:
            out = pickle.load(f)
        return out

    def GenPerfTimeline(self, input_core, input_thread, dump_path=None, clk_diff=0):
        if len(self.target_ths) == 0:
            print("Warring, target thread list is empty. No tid mapping will be mapped. You might not provide thread pool information to "
                  "the postprocessor. Considering thp_info_addr to the post processor. ")
        ret = []
        # Thread view:
        thread_view = []
        for tid in input_thread.keys():
            # if not int(tid) in self.target_ths:
            #     continue
            event_list = input_thread[int(tid)]
            last_event = ['e',None]
            for idx, event in enumerate(event_list): # event: [tag, [core, time, prev_tid, prev_name, next_tid, next_name]]
                cat = "B" if event[0] == 'b' else "E"
                temp = {"name":str(event[1][0]),"pid":20,"tid":int(tid),"ts":(event[1][1]+clk_diff)/1000,"ph":cat, "cat":"Thread view"}
                thread_view.append(temp)
                # print(temp)
        ret += thread_view

        # Core view
        core_view = []
        for core in input_core.keys():
            event_list = input_core[core]
            for idx, event in enumerate(event_list): # event [core, time, prev_tid, prev_name, next_tid, next_name]
                temp_prev = {"name":str(event[2]), "pid":21, "tid":int(event[0]), "ts":(event[1]+clk_diff)/1000, "ph":"E", "cat":"Core view"
                             ,"args":{"process_name":event[3]}}
                temp_next = {"name":str(event[4]), "pid":21, "tid":int(event[0]), "ts":(event[1]+clk_diff)/1000, "ph":"B", "cat":"Core view"
                             ,"args":{"process_name":event[5]}}
                core_view.append(temp_prev)
                core_view.append(temp_next)
        ret += core_view
        ret.append({"name":"process_name", "ph":"M", "pid":20, "args":{"name":"Thread view Sched info"}})
        ret.append({"name":"process_name", "ph":"M", "pid":21, "args":{"name":"Core view Sched info"}})

        if dump_path is not None:
            with open(dump_path, 'w', encoding="utf-8") as f:
                json.dump(ret, f, indent=4)

        return ret

    def PerfDataToTimeline(self, data_path, time_boundary=None, output_path=None):
        if not os.path.exists(data_path):
            raise Exception("Perf record data file does is not exist!")
        if not PathCheck([data_path]):
            raise Exception("Perf record data does is not valid!")

        time_opt = ""
        if time_boundary is not None:
            time_begin, time_end = time_boundary
            if self.clk_source == "MON" and time_begin - time.clock_gettime(time.CLOCK_MONOTONIC) > 0:
                # The time boundary is using clock real time
                time_begin -= self.clk_diff
                time_end -= self.clk_diff
            time_begin_s = int(time_begin/(1000*1000*1000))
            time_begine_ns = int(time_begin - time_begin_s*(1000*1000*1000))
            time_end_s = int(time_end / (1000 * 1000 * 1000))
            time_end_ns = int(time_end - time_end_s * (1000 * 1000 * 1000))
            time_opt = f" --time {time_begin_s}.{time_begine_ns},{time_end_s}.{time_end_ns}"

        # This perf script will run perf-script.py and generate a pickle obj sched_info
        if not os.path.isfile(data_path):
            raise Exception("Perf record data does is not file!")

        if os.getenv('PERF_EXEC_PATH') is not None and not os.path.exists(os.path.join(os.environ['PERF_EXEC_PATH'], '/scripts/python/Perf-Trace-Util/lib/Perf/Trace')):
            raise Exception("Perf execution path does not exist!")

        ret = os.system(f"perf script -i {data_path} -s {os.path.join(self.abs_path, 'perf-script.py')}{time_opt}")
        if ret :
            raise Exception("Perf command error!")

        output_core, output_thread = self.LoadPerfData(os.path.join(self.abs_path, "sched_info"))

        timeline_dict = self.GenPerfTimeline(output_core, output_thread,
                                             output_path, self.clk_diff)

        return timeline_dict

class EbpfMapper(PerfMapper):
    def __init__(self, thp_info_folder=None):
        super().__init__(thp_info_folder)
        self.task_state = {
            0x0:"TASK_RUNNING",
            0x1:"TASK_INTERRUPTABLE",
            0x2:"TASK_UNINTERRUPTIBLE",
            0x4:"__TASK_STOPPED",
            0x8:"__TASK_TRACED",
            0x10:"EXIT_DEAD",
            0x20:"EXIT_ZOMBIE",
            0x0040:"TASK_PARKED",
            0x0080:"TASK_DEAD",
            0x0100:"TASK_WAKEKILL",
            0x0200:"TASK_WAKING",
            0x0400:"TASK_NOLOAD",
            0x0800:"TASK_NEW",
            0x102:"TASK_KILLABLE",
            0x104:"TASK_STOPPED",
            0x108:"TASK_TRACED",
            0x402:"TASK_IDLE",
            0x3:"TASK_NORMAL",
            0x7f:"TASK_REPORT",
        }

    def ParseEbpfData(self, path_to_ebpf_data, time_boundary=None):
        output_core = {}
        output_thread = {}

        with open(path_to_ebpf_data, 'r') as f:
            lines = f.readlines()

            clk_source = [int(i) for i in lines.pop(0).split(',')]
            self.clk_diff = clk_source[1] - clk_source[0]

            for line in lines:
                segments = line.split('\n')[0].split(',')
                # print(segments)
                # 0 - core, 1 - time, 2 - wp ar, 3 - idx, 4 - prv_state, 5 - prv_pid, 6 - next_pid, 7 - PMC reading
                data = segments
                data[1] = int(data[1])
                if time_boundary is not None:
                    if data[1] < time_boundary[0] or data[1] > time_boundary[1]:
                        continue
                data[4] = f"{hex(int(data[4]))}-{self.task_state[int(data[4])] if self.task_state.get(int(data[4])) is not None else 'Unknown'}"

                if data[0] in output_core.keys():
                    output_core[data[0]].append(data)
                else:
                    output_core[data[0]] = [data]

                if data[5] in output_thread.keys():
                    output_thread[data[5]].append(['e', data])
                else:
                    output_thread[data[5]] = [['e', data]]

                if data[6] in output_thread.keys():
                    output_thread[data[6]].append(['b', data])
                else:
                    output_thread[data[6]] = [['b', data]]
        return output_core, output_thread

    def CoreThreadDataToTimeline(self, path_to_ebpf_data, dump_path=None, time_boundary=None):
        input_core, input_thread = self.ParseEbpfData(path_to_ebpf_data, time_boundary)

        if len(self.target_ths) == 0:
            print("Warring, target thread list is empty. No tid mapping will be mapped. You might not provide thread pool information to "
                  "the postprocessor. Considering thp_info_addr to the post processor. ")

        ret = []
        # Thread view
        thread_view = []
        for tid in input_thread.keys():
            if len(self.target_ths) > 0 and not int(tid) in self.target_ths:
                continue
            event_list = input_thread[tid]
            for idx, event in enumerate(event_list): # event: [tag, [# 0 - core, 1 - time, 2 - wp ar, 3 - idx, 4 - prv_state, 5 - prv_pid, 6 - next_pid]]
                cat = "B" if event[0] == 'b' else "E"
                temp = {"name":str(event[1][0]),"pid":40,"tid":int(tid),"ts":float(event[1][1])/1000,"ph":cat,"cat":"EBPF Thread view",
                        "args":{"Warp":event[1][2],"Curidx":event[1][3]}}
                if event[0] == 'b':
                    temp["args"]["PrevThreadEnddingState"] = event[1][4]
                    temp["args"]["Begin PMC"] = event[1][7]
                else:
                    temp["args"]["CurrThreadEnddingState"] = event[1][4]
                    temp["args"]["End PMC"] = event[1][7]

                thread_view.append(temp)
                # print(temp)
        ret += thread_view

        # Core view
        core_view = []
        for core in input_core.keys():
            event_list = input_core[core]
            for idx, event in enumerate(event_list):  # event [core, time, prev_tid, prev_name, next_tid, next_name]
                temp_prev = {"name": str(event[5]), "pid": 41, "tid": int(event[0]), "ts": float(event[1])/1000,
                             "ph": "E", "cat": "EBPF Core view"
                             ,"args":{"End PMC":event[7]}
                             }
                temp_next = {"name": str(event[6]), "pid": 41, "tid": int(event[0]), "ts": float(event[1])/1000,
                             "ph": "B", "cat": "EBPF Core view"
                    ,"args": {"state":f"{str(event[5])}-{event[4]}","Warp":event[2],"Curidx":event[3]
                             ,"Begin PMC":event[7]
                               }}
                core_view.append(temp_prev)
                core_view.append(temp_next)
        ret += core_view

        ret.append({"name": "process_name", "ph": "M", "pid": 40, "args": {"name": "EBPF Thread view Sched info"}})
        ret.append({"name": "process_name", "ph": "M", "pid": 41, "args": {"name": "EBPF Core view Sched info"}})

        if dump_path is not None:
            with open(dump_path, 'w', encoding="utf-8") as f:
                json.dump(ret, f, indent=4)

        return ret

class PostProcessor:

    def __init__(self, main_args, tma_cal, thp_info_addr=None, step_state_addr=None, skip=None):
        self.log_folder = main_args.l
        self.thp_info_addr = main_args.t
        if step_state_addr is None:
            self.step_state_addr = os.path.join(main_args.l, "step_state")
        else:
            self.step_state_addr = step_state_addr

        # TF timeline
        self.timeline_worker = TimelineWorker(output_folder=main_args.l)
        self.timeline = None
        self.timeline_json = None
        self.time_boundary = None
        self.tf_timeline_json = None

        # TF StepState
        self.step_state = None
        self.sorted_node_state = None # node state sorted in increasing order of starting times
        self.thread_name = None # Inter op thread id + name

        # MTMC logs
        self.perfmon_parser = PerfmonParser(tma_cal, main_args.c)
        self.log_dict = {} # key: mtmc_raw data name, val: List:[SingleLog]
        self.log_dict_with_int_prefix = {} # key: mtmc_raw data name, val dict{int_prefix : List:[SingleLog]}
        self.config_addr = main_args.c

        # Extra configs:
        if skip is not None:
            self.skip = skip.split(",")
        else:
            self.skip = []

        # eBPF
        self.eBPF = args.e

    def ProcessTFTimeline(self):
        if self.step_state_addr is None or not os.path.exists(self.step_state_addr):
            raise Exception("Step_state_addr does not exist. Stop process timeline.")

        # Generate TF style timeline
        self.timeline = self.timeline_worker.StepState2Timeline(self.step_state_addr)
        self.timeline_json = json.loads(self.timeline)

        # Convert Original tf timeline from us to ns
        for event in self.timeline_json['traceEvents']:
            keys = event.keys()
            if 'ts' in keys:
                event['ts'] = int(event['ts'])
            if 'dur' in keys:
                event['dur'] = int(event['dur'])

        # Calculate TF style timeline boundary, and conver the result from us to ns
        self.time_boundary = [i * 1000 for i in self.timeline_worker.GetMinMaxTime(self.step_state_addr)]
        print(f"Time boundary {self.time_boundary[0], self.time_boundary[1]}")

    def ProcessStepState(self):
        if self.step_state_addr is None or not os.path.exists(self.step_state_addr):
            raise Exception("Step_state_addr does not exist. Stop process step state.")

        self.step_state = self.timeline_worker.ReadStepState(self.step_state_addr)
        nodes = self.step_state.dev_stats[0].node_stats

        self.thread_name = {item.thread_id : "InterOp" for item in nodes}

        # self.thread_name = {key:value for key, value in self.step_state.dev_stats[0].thread_names.items()}
        self.sorted_node_state = []
        for node in nodes:
            node.all_start_micros *= 1000
            node.all_end_rel_micros *= 1000
            self.sorted_node_state.append((node.all_start_micros, node))

        self.sorted_node_state = sorted(self.sorted_node_state, key=lambda x:x[0])

        print("Done process step state")

    def ReadRawLogs(self, file_name=None, use_time_boundary=True):
        file_name_partial = None
        if file_name is None:
            file_name_partial = "mtmc_raw_"
        names = os.listdir(self.log_folder)
        for name in names:
            if file_name == name or (file_name_partial is not None and file_name_partial in name):
                print(f"Process raw log: {name}")
                self.log_dict[name] = []
                self.log_dict_with_int_prefix[name] = {}
                with open(os.path.join(self.log_folder, name), 'r') as f:
                    logs = f.readlines()
                    for line in logs:
                        sig_prof = line.split(',')
                        if len(sig_prof) < kSingleLogLen:
                            print("Skip sig_prof: ", sig_prof)
                            continue
                        sig_log = SingleLog(sig_prof)

                        # If use_time_boundary is true, we only store raw logs that within the boundary
                        if use_time_boundary and self.time_boundary is not None:
                            if sig_log.time_begin < self.time_boundary[0] or sig_log.time_end > self.time_boundary[1]:
                                continue
                        self.log_dict[name].append(sig_log)
                        if not sig_log.int_prefix in self.log_dict_with_int_prefix[name].keys():
                            self.log_dict_with_int_prefix[name][sig_log.int_prefix] = [sig_log]
                        else:
                            self.log_dict_with_int_prefix[name][sig_log.int_prefix].append(sig_log)

    def ExportRawLogs(self):
        if self.log_dict is None:
            raise Exception("ExportRawLogs failed due to log_dict is None.")

        temp_output = []
        for key, value in self.log_dict.items():
            name = key
            for sig_log in value:
                name = "raw_event"
                if sig_log.prefix is not None and sig_log.prefix is not "":
                    name = sig_log.prefix
                temp_output.append({
                    "name": name, "ph":"X", "cat":"Raw MTMC logs",
                    # Here the timestemp is converted to us from ns
                    "ts" : np.float(sig_log.time_begin)/1000, "dur":np.float(sig_log.time_end - sig_log.time_begin)/1000,
                    "pid": name, "tid":sig_log.tid,
                    "args" : {"name" : f"{key}"}
                })

        with open(os.path.join(self.log_folder, "raw_log.json"), 'w',encoding="utf-8") as f:
            json.dump(temp_output, f, indent=4)

    def ExportProcessedLogs(self, mapped_logs_dict=None, sched_dict=None, nontftl_log_dict=None):

        export_raw_log = None
        if mapped_logs_dict is None:
            mapped_logs_dict = self.log_dict
            export_raw_log = True
            print("Will export raw log")
        dump_dict = []

        # Process step_state to json
        if self.sorted_node_state is not None:
            temp_list = []
            for _,node_state in self.sorted_node_state:
                start = float(node_state.all_start_micros) / 1000
                dur = float(node_state.all_end_rel_micros) / 1000
                name = node_state.node_name.split(':')[0]
                op = node_state.node_name.split(':')[1] if len(node_state.node_name.split(':')) is 2 else node_state.node_name.split(':')[
                    0]
                ktid = node_state.thread_id
                timeline_label = node_state.timeline_label

                temp1 = {"name": op, "ts": start, "dur": dur, "tid": ktid,
                         "pid": 10,
                         "cat": "Detailed node state",
                         "ph": "X", "args": {"node_name": name, "step_id": timeline_label.split(",")[0].split("=")[1],
                                             "timeline_lable": timeline_label} }
                temp_list.append(temp1)

            dump_dict += temp_list
            dump_dict.append({"name":"process_name", "ph":"M", "pid":10, "args":{"name":"Detailed Node State"}})
            dump_dict.append({"name":"process_name", "ph":"M", "pid":15, "args":{"name":"MTMC InterOP Log"}})

        # TF Original timeline
        if self.timeline_json is not None:
            dump_dict += self.timeline_json['traceEvents']

        # Mapped Log
        if mapped_logs_dict is not None:
            temp_list = []
            for key, value in mapped_logs_dict.items():
                for single_log in value:
                    # Try to get scheduler's Op name
                    if export_raw_log:
                        parent_name = single_log.prefix
                    else:
                        parent_name = f"Unknown_{single_log.parent_pthread_id}"
                    args_dict = {}
                    args_dict["pthread_id"] = single_log.pthread_id
                    args_dict["parent_pthread_id"] = single_log.parent_pthread_id
                    args_dict["parent_tid"] = single_log.parent_tid
                    args_dict["parent_sched_time"] = single_log.parent_tid
                    args_dict["int_prefix"] = single_log.int_prefix
                    args_dict["prefix"] = single_log.prefix
                    args_dict["metric_group"] = single_log.cfg_idx
                    if single_log.schedular is not None:
                        name_split = single_log.schedular.node_name.split(':')
                        parent_name = name_split[1] if len(name_split) is 2 else name_split[0]
                        node_name = name_split[0]
                        step_id = int(single_log.schedular.timeline_label.split(",")[0].split("=")[1])
                        args_dict["step_id"] = step_id
                        args_dict["node_name"] = node_name

                    temp_pid = key.split("_")[-1]
                    temp_tid = single_log.tid

                    prefix_list = single_log.prefix.split("~")
                    if prefix_list[0] == "INTEROP":
                        parent_name = prefix_list[2]
                        temp_tid = single_log.pthread_id # Here use pthread_id instead of tid to better compare with tensorflow timeline
                        temp_pid = 15

                    temp_dict = {
                        "name": parent_name, "ph": "X", "cat": "MTMC logs",
                        # Here the timestemp is converted to us from ns
                        "ts": np.float(single_log.time_begin) / 1000,
                        "dur": np.float(single_log.time_end - single_log.time_begin) / 1000,
                        "pid": temp_pid, "tid": temp_tid,
                        "args": args_dict
                    }

                    # Calculate TMAM
                    if self.perfmon_parser.always_topdown:
                        tmam_names, output_tmam = self.perfmon_parser.CalTopDown(single_log.b_events[0],
                                                                                 single_log.b_events[1],
                                                                                 single_log.b_events[2],
                                                                                 single_log.e_events[0],
                                                                                 single_log.e_events[1],
                                                                                 single_log.e_events[2])
                        assert(len(tmam_names) == len(output_tmam))
                        for tmam_idx, name in enumerate(tmam_names):
                            temp_dict["args"][name] = output_tmam[tmam_idx]


                    # Calulate other pmu events
                    self.perfmon_parser.CalPmu(pmu_begin=single_log.b_events, pmu_end=single_log.e_events,
                                               dict_to_append=temp_dict["args"], cfg_idx=single_log.cfg_idx,
                                               cnsts=single_log.cnst)

                    temp_list.append(temp_dict)

            dump_dict += temp_list

        nontf_pid = 33
        if nontftl_log_dict is not None:
            temp_list = []
            for key, value in nontftl_log_dict.items():

                interop_logs = value[0]
                intraop_logs = value[1]

                dump_dict.append({"name": "process_name", "ph": "M", "pid": nontf_pid, "args": {"name": f"Nontf-{key}-InterOp"}})
                for single_log in interop_logs:
                    args_dict = {}
                    args_dict["pthread_id"] = single_log.pthread_id
                    args_dict["parent_pthread_id"] = single_log.parent_pthread_id
                    args_dict["parent_tid"] = single_log.parent_tid
                    args_dict["parent_sched_time"] = single_log.parent_tid
                    args_dict["int_prefix"] = single_log.int_prefix
                    args_dict["prefix"] = single_log.prefix
                    prefix_list = single_log.prefix.split("~")
                    if len(prefix_list) < 3:
                        parent_name = single_log.prefix
                    else:
                        parent_name = prefix_list[2]
                    temp_tid = single_log.tid
                    temp_pid = nontf_pid

                    temp_dict = {
                        "name": parent_name, "ph": "X", "cat": "Nontf InterOp",
                        # Here the timestemp is converted to us from ns
                        "ts": np.float(single_log.time_begin) / 1000,
                        "dur": np.float(single_log.time_end - single_log.time_begin) / 1000,
                        "pid": temp_pid, "tid": temp_tid,
                        "args": args_dict
                    }

                    # Calculate TMAM
                    if self.perfmon_parser.always_topdown:
                        tmam_names, output_tmam = self.perfmon_parser.CalTopDown(single_log.b_events[0],
                                                                                 single_log.b_events[1],
                                                                                 single_log.b_events[2],
                                                                                 single_log.e_events[0],
                                                                                 single_log.e_events[1],
                                                                                 single_log.e_events[2])

                        assert (len(tmam_names) == len(output_tmam))
                        for tmam_idx, name in enumerate(tmam_names):
                            temp_dict["args"][name] = output_tmam[tmam_idx]

                    self.perfmon_parser.CalPmu(pmu_begin=single_log.b_events, pmu_end=single_log.e_events,
                                               dict_to_append=temp_dict["args"], cfg_idx=single_log.cfg_idx,
                                               cnsts=single_log.cnst)

                    temp_list.append(temp_dict)

                nontf_pid+=1
                dump_dict.append({"name": "process_name", "ph": "M", "pid": nontf_pid, "args": {"name": f"Nontf-{key}-IntraOp"}})
                for single_log in intraop_logs:
                    args_dict = {}
                    args_dict["pthread_id"] = single_log.pthread_id
                    args_dict["parent_pthread_id"] = single_log.parent_pthread_id
                    args_dict["parent_tid"] = single_log.parent_tid
                    args_dict["parent_sched_time"] = single_log.parent_tid
                    args_dict["int_prefix"] = single_log.int_prefix
                    args_dict["prefix"] = single_log.prefix
                    prefix_list = single_log.prefix.split("~")
                    if (len(prefix_list) < 3):
                        parent_name = single_log.prefix
                    else:
                        parent_name = prefix_list[2]
                    temp_tid = single_log.tid
                    temp_pid = nontf_pid

                    temp_dict = {
                        "name": parent_name, "ph": "X", "cat": "Nontf IntraOp",
                        # Here the timestemp is converted to us from ns
                        "ts": np.float(single_log.time_begin) / 1000,
                        "dur": np.float(single_log.time_end - single_log.time_begin) / 1000,
                        "pid": temp_pid, "tid": temp_tid,
                        "args": args_dict
                    }

                    # Calculate TMAM
                    if self.perfmon_parser.always_topdown:
                        tmam_names, output_tmam = self.perfmon_parser.CalTopDown(single_log.b_events[0],
                                                                                 single_log.b_events[1],
                                                                                 single_log.b_events[2],
                                                                                 single_log.e_events[0],
                                                                                 single_log.e_events[1],
                                                                                 single_log.e_events[2])

                        assert (len(tmam_names) == len(output_tmam))
                        for tmam_idx, name in enumerate(tmam_names):
                            temp_dict["args"][name] = output_tmam[tmam_idx]

                    self.perfmon_parser.CalPmu(pmu_begin=single_log.b_events, pmu_end=single_log.e_events,
                                               dict_to_append=temp_dict["args"], cfg_idx=single_log.cfg_idx,
                                               cnsts=single_log.cnst)

                    temp_list.append(temp_dict)

                nontf_pid+=1

            dump_dict += temp_list

        if sched_dict is not None:
            dump_dict += sched_dict

        with open(os.path.join(self.log_folder, "processed_log.json"), 'w', encoding="utf-8") as f:
            json.dump(dump_dict, f, indent=4)

    def ProcessMTMCLogFiles(self):

        start_time = time.time()

        # Tensorflow 1.15.0 profiler information (App info)
        self.ProcessStepState()
        self.ProcessTFTimeline()

        # Sched info(System info) from guard sampler
        if "ctx_switch" in self.skip:
            sched_dict = None
        else:
            pm = PerfMapper(self.thp_info_addr)
            pm.CalClock(os.path.join(self.log_folder, "timecal.txt"))
            sched_dict = pm.PerfDataToTimeline(os.path.join(self.log_folder, "perf.data"),
                                  time_boundary=self.time_boundary,
                                  output_path=os.path.join(self.log_folder, "perf_sched.json"))

        # eBPF context switch + PMC
        if self.eBPF is not None:
            ebpf_mapper = EbpfMapper()
            sched_dict = ebpf_mapper.CoreThreadDataToTimeline(self.eBPF, time_boundary=self.time_boundary)
            print("Done generate ebpf info")

        tf_end_time = time.time()
        print(f"Tensorflow log processing time: {tf_end_time - start_time}s")

        # TMA Logs(uArch info) from mtmc
        self.ReadRawLogs()
        self.ExportRawLogs()

        raw_log_end_time = time.time()
        print(f"Raw log processing time: {raw_log_end_time - tf_end_time}")

        # Map TMA logs to timeline
        pool = Pool(cores)
        tmm = TimelineMapper(self.sorted_node_state, self.thread_name)

        mapped_log_dict = {}
        nontftl_log_dict = {}
        if not tmm.valid:
            mapped_log_dict = None
        else:
            ## Not using log_dict_with_int_prefix. Parallel across individual log
            # for name, raw_log in self.log_dict.items():
            #     st = time.time()
            #     mapped_logs, recheck_logs, failed_logs = tmm.Search(raw_log, pool)
            #     # mapped_logs, recheck_logs, failed_logs = tmm.SearchSingleThread(raw_log)
            #     mapped_log_dict[name.split("_")[-1]] = mapped_logs + recheck_logs + failed_logs
            #     end = time.time()
            #     print(f"{name} search time: {end - st}")

            ## Using log_dict_with_int_prefix. Parallel across int_prefix. Sequential search individual log within an int_prefix
            for name, int_prefix_raw_log in self.log_dict_with_int_prefix.items():
                st = time.time()
                # mapped_logs, recheck_logs, failed_logs = tmm.Search(raw_log, pool)
                mapped_logs, recheck_logs, failed_logs, interop_logs = tmm.SearchWithIntPrefixRawLogs(pool, int_prefix_raw_log)
                mapped_log_dict[name.split("_")[-1]] = mapped_logs + recheck_logs + failed_logs + interop_logs
                end = time.time()
                print(f"{name} search time: {end - st}")

                # Generalize Non-TFtimeline Logs:
                nontftl_interop_logs, nontftl_intraop_logs = tmm.GeneralizeWithIntPrefixRawLogs(pool, int_prefix_raw_log)
                nontftl_log_dict[name.split("_")[-1]] = [nontftl_interop_logs, nontftl_intraop_logs]
                st = time.time()

        self.ExportProcessedLogs(mapped_log_dict, sched_dict, nontftl_log_dict)

        print(f"Total time {time.time() - start_time}")

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

if __name__ == "__main__":

    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument("-l", type=str, required=True, default=None, help="Path to the log collected by mtmc collector")
    arg_parser.add_argument("-t", type=str, required=False, default=None, help="Path to the log thread pool information")
    arg_parser.add_argument("-c", type=str, required=True, default=None, help="Path to the mtmc config")
    arg_parser.add_argument("-m", type=str, default="tf1", help="Post processing mode: tf1 - Tensorflow 1.15.0 full log. normal - mtmc without tf information")
    arg_parser.add_argument("-r", type=str, default=None, help="Comma separated mtmc log names that will be processed")
    arg_parser.add_argument("--skip", type=str, default=None, help="Comma separated list of analysis that will be ignore:\n"
                                                                   "ctx_switch: Context switch event collected by guard sampler")
    arg_parser.add_argument("-e", type=str, default=None, help="Path to ebpf ctx sw file output")

    args = arg_parser.parse_args()

    path_valid = True

    path_valid &= PathCheck([args.l, args.t, args.c])

    if not os.path.isdir(args.l):
        print("Failed. -l should be a directory, not a file")
        path_valid = False
    if args.t and not os.path.isdir(args.t):
        print("Failed. -t should be a directory, not a file")
        path_valid = False
    if not os.path.isfile(args.c):
        print("Failed. -c should be a file, not a directory")
        path_valid = False

    if path_valid:
        pp = PostProcessor(args,
                           tma_cal=tc.SprTmaCal(),
                           thp_info_addr=args.t,
                           skip=args.skip)
        if args.m == "tf1":
            pp.ProcessMTMCLogFiles()
        elif args.m == "normal":
            print(f"Process raw log only: [{args.r}]")
            if args.r is not None:
                for sig_name in args.r.split(','):
                    pp.ReadRawLogs(sig_name)
                pp.ExportProcessedLogs()
            else:
                print("if -m is set to normal, you should use -r to specify desired raw log address to process")
        print("Done post processing")
    else:
        print("Failed post processing due to invalid path")

