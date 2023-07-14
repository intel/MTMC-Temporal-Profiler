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


import json
import os.path
import tma_cal as tc
import requests
from perfmon_parser import PerfmonParser
import argparse

def parse_json(path):
    with open(path, 'r') as f:
        return json.load(f)

def ParseSpanToTimelineTF(file, cfg_file, output_file):
    perfmon_parser = PerfmonParser(tc.SprTmaCal(), cfg_file)

    dump_dict = []
    span_name_dict = {}

    dump_dict.append({"name": "process_name", "ph": "M", "pid": 10, "args": {"name": f"InterOp Pool"}})
    dump_dict.append({"name": "process_name", "ph": "M", "pid": 11, "args": {"name": f"IntraOp Pool"}})
    dump_dict.append({"name": "process_name", "ph": "M", "pid": 9, "args": {"name": f"Detailed node state"}})

    for idx in range(len(file['data'])):
        for span in file['data'][idx]['spans']:
            span_name = span['operationName']
            span_name_list = span_name.split("~")
            span_id = span['spanID']
            span_name_dict[span_id] = span_name_list
            if len(span_name_list) != 6:
                print(span_name_list)
                continue
            pool_type = span_name_list[0]
            op_name = span_name_list[1]
            op_type = span_name_list[2]
            op_ctx_hash = span_name_list[3]
            op_inputs = span_name_list[4].split('|')
            op_step_id = span_name_list[5] if span_name_list[5] != '' else -1
            start_time = span['startTime']
            duration = span['duration']
            for log_field in span['logs'][0]['fields']:
                if log_field['key'] == 'tid':
                    tid = log_field['value']
                elif log_field['key'] == 'pthreadid':
                    pthread_id = int(log_field['value']) & 0xffffffff
                elif log_field['key'] == 'int_prefix':
                    int_prefix = log_field['value']
                elif log_field['key'] == 'parent_tid_pthreadid_sched_time':
                    parent_info_col = log_field['value']
                    parent_tid, parent_pthread_id, sched_time = parent_info_col.split("-*-")
                    parent_pthread_id = int(parent_pthread_id) & 0xffffffff
                elif log_field['key'] == 's_coreid_prefix_num_event':
                    b_event_col = log_field['value']
                    b_core, b_prefix, b_events_num = b_event_col.split("-")
                elif log_field['key'] == 'e_coreid_prefix_num_event':
                    e_event_col = log_field['value']
                    e_core, e_prefix, e_events_num = e_event_col.split("-")
                elif log_field['key'] == 'b_events':
                    b_events = [int(i) for i in log_field['value'].split("-")]
                elif log_field['key'] == 'e_events':
                    e_events = [int(i) for i in log_field['value'].split("-")]
                elif log_field['key'] == 'cfg_id':
                    cfg_id = log_field['value']
                elif log_field['key'] == 'mux_id':
                    mux_id = log_field['value']
                elif log_field['key'] == 'cnsts':
                    cnsts = [int(i) for i in log_field['value'].split("-*-") if i != '']

            if cfg_id == -1:
                parser_cfg_id = mux_id
                # print("Intra-Process Event multiplexing.")
            else:
                parser_cfg_id = cfg_id
                # print("Inter-Process Event multiplexing.")

            if op_name == "default" or op_type == "default":
                continue

            args_dict = {
                "node_name": op_name,
                "pthread_id": pthread_id,
                "parent_pthread_id": parent_pthread_id,
                "parent_tid": parent_tid,
                "parent_sched_time": sched_time,
                "step_id": op_step_id
            }

            temp_dict = {
                "name": op_type, "ph": "X", "cat": "MTMC logs",
                # Here the timestemp is converted to us from ns
                "ts": start_time,
                "dur": duration,
                "tid": tid,
                "pid": 10 if pool_type == "INTEROP" else 11,
                "args": args_dict
            }

            perfmon_parser.CalPmu(b_events, e_events, temp_dict["args"], cfg_idx=parser_cfg_id, cnsts=cnsts, dur_ms=float(duration)/1000)

            dump_dict.append(temp_dict)

            # For auto analysis
            if span_name_list[0] == "INTEROP":
                # Add Detailed node state
                dtl_node_dict = {
                    "name": op_type,
                    "ts": start_time,
                    "dur": duration,
                    "pid": 9,
                    "tid": tid,
                    "cat": "Detailed node state",
                    "ph": "X",
                    "args": {
                        "node_name": op_name,
                        "step_id": op_step_id,
                        "input_nodes": op_inputs
                    }
                }
                dump_dict.append(dtl_node_dict)


    with open(output_file, 'w', encoding="utf-8") as f:
        json.dump(dump_dict, f, indent=4)

def ParseSpanToTimelinePT(file, cfg_file, output_file):
    perfmon_parser = PerfmonParser(tc.SprTmaCal(), cfg_file)

    dump_dict = []
    span_name_dict = {}
    detailed_dict = {}
    op_name_list = []

    dump_dict.append({"name": "process_name", "ph": "M", "pid": 10, "args": {"name": f"InterOp Pool"}})
    dump_dict.append({"name": "process_name", "ph": "M", "pid": 11, "args": {"name": f"IntraOp Pool"}})
    dump_dict.append({"name": "process_name", "ph": "M", "pid": 9, "args": {"name": f"Detailed node state"}})

    for idx in range(len(file['data'])):
        for span in file['data'][idx]['spans']:
            span_name = span['operationName']
            span_name_list = span_name.split("~")
            span_id = span['spanID']
            span_name_dict[span_id] = span_name_list
            if len(span_name_list) != 6:
                print(span_name_list)
                continue
            pool_type = span_name_list[0]
            op_name = span_name_list[1]
            op_type = span_name_list[2]
            op_ctx_hash = span_name_list[3]
            op_inputs = span_name_list[4].split('|')
            op_step_id = span_name_list[5] if span_name_list[5] != '' else -1
            start_time = span['startTime']
            duration = span['duration']
            for log_field in span['logs'][0]['fields']:
                if log_field['key'] == 'tid':
                    tid = log_field['value']
                elif log_field['key'] == 'pthreadid':
                    pthread_id = int(log_field['value']) & 0xffffffff
                elif log_field['key'] == 'int_prefix':
                    int_prefix = log_field['value']
                elif log_field['key'] == 'parent_tid_pthreadid_sched_time':
                    parent_info_col = log_field['value']
                    parent_tid, parent_pthread_id, sched_time = parent_info_col.split("-*-")
                    parent_pthread_id = int(parent_pthread_id) & 0xffffffff
                elif log_field['key'] == 's_coreid_prefix_num_event':
                    b_event_col = log_field['value']
                    b_core, b_prefix, b_events_num = b_event_col.split("-")
                elif log_field['key'] == 'e_coreid_prefix_num_event':
                    e_event_col = log_field['value']
                    e_core, e_prefix, e_events_num = e_event_col.split("-")
                elif log_field['key'] == 'b_events':
                    b_events = [int(i) for i in log_field['value'].split("-")]
                elif log_field['key'] == 'e_events':
                    e_events = [int(i) for i in log_field['value'].split("-")]
                elif log_field['key'] == 'cfg_id':
                    cfg_id = log_field['value']
                elif log_field['key'] == 'mux_id':
                    mux_id = log_field['value']
                elif log_field['key'] == 'cnsts':
                    cnsts = [int(i) for i in log_field['value'].split("-*-") if i != '']

            if cfg_id == -1:
                parser_cfg_id = mux_id
                # print("Intra-Process Event multiplexing.")
            else:
                parser_cfg_id = cfg_id
                # print("Inter-Process Event multiplexing.")

            if op_name == "default" or op_type == "default":
                continue

            args_dict = {
                "node_name": op_name,
                "pthread_id": pthread_id,
                "parent_pthread_id": parent_pthread_id,
                "parent_tid": parent_tid,
                "parent_sched_time": sched_time,
                "step_id": op_step_id
            }

            temp_dict = {
                "name": op_type, "ph": "X", "cat": "MTMC logs",
                # Here the timestemp is converted to us from ns
                "ts": start_time,
                "dur": duration,
                "tid": tid,
                "pid": 10 if pool_type == "INTEROP" else 11,
                "args": args_dict
            }

            perfmon_parser.CalPmu(b_events, e_events, temp_dict["args"], cfg_idx=parser_cfg_id, cnsts=cnsts, dur_ms=float(duration)/1000)

            dump_dict.append(temp_dict)

            # For auto analysis
            if span_name_list[0] == "INTEROP":
                # Add Detailed node state
                dtl_node_dict = {
                    "name": op_type,
                    "ts": start_time,
                    "dur": duration,
                    "pid": 9,
                    "tid": tid,
                    "cat": "Detailed node state",
                    "ph": "X",
                    "args": {
                        "node_name": op_name,
                        "step_id": op_step_id,
                        "input_nodes": op_inputs
                    }
                }
                if (op_name.isdigit()):
                    op_name_list.append(int(op_name))
                    key_step = op_step_id
                    key_name = int(op_name)
                    if key_step not in detailed_dict :
                        detailed_dict[key_step] = {}
                        detailed_dict[key_step][key_name] = dtl_node_dict
                    else :
                        if key_name not in detailed_dict[key_step] :
                            detailed_dict[key_step][key_name] = dtl_node_dict
                        else :
                            if detailed_dict[key_step][key_name]["dur"] <= duration :
                                detailed_dict[key_step][key_name] = dtl_node_dict
                            
    l = sorted(list(set(op_name_list)))
    for v in detailed_dict.values() :
        for i in range(1, len(l)) :
            e_v = v[int(l[i])]
            e_v["args"]["input_nodes"] = [l[i - 1]]
            dump_dict.append(e_v)

    with open(output_file, 'w', encoding="utf-8") as f:
        json.dump(dump_dict, f, indent=4)

def ParseSpanToTimeline(file, cfg_file, output_file):

    perfmon_parser = PerfmonParser(tc.SprTmaCal(), cfg_file)

    dump_dict = []
    span_name_dict = {}

    dump_dict.append({"name": "process_name", "ph": "M", "pid": 10, "args": {"name": f"InterOp Pool"}})
    dump_dict.append({"name": "process_name", "ph": "M", "pid": 11, "args": {"name": f"IntraOp Pool"}})
    dump_dict.append({"name": "process_name", "ph": "M", "pid": 9, "args": {"name": f"Detailed node state"}})

    for idx in range(len(file['data'])):
        for span in file['data'][idx]['spans']:
            span_name = span['operationName']
            span_id = span['spanID']
            start_time = span['startTime']
            duration = span['duration']
            for log_field in span['logs'][0]['fields']:
                if log_field['key'] == 'tid':
                    tid = log_field['value']
                elif log_field['key'] == 'pthreadid':
                    pthread_id = int(log_field['value']) & 0xffffffff
                elif log_field['key'] == 'int_prefix':
                    int_prefix = log_field['value']
                elif log_field['key'] == 'parent_tid_pthreadid_sched_time':
                    parent_info_col = log_field['value']
                    parent_tid, parent_pthread_id, sched_time = parent_info_col.split("-*-")
                    parent_pthread_id = int(parent_pthread_id) & 0xffffffff
                elif log_field['key'] == 's_coreid_prefix_num_event':
                    b_event_col = log_field['value']
                    b_core, b_prefix, b_events_num = b_event_col.split("-")
                elif log_field['key'] == 'e_coreid_prefix_num_event':
                    e_event_col = log_field['value']
                    e_core, e_prefix, e_events_num = e_event_col.split("-")
                elif log_field['key'] == 'b_events':
                    b_events = [int(i) for i in log_field['value'].split("-")]
                elif log_field['key'] == 'e_events':
                    e_events = [int(i) for i in log_field['value'].split("-")]
                elif log_field['key'] == 'cfg_id':
                    cfg_id = log_field['value']
                elif log_field['key'] == 'mux_id':
                    mux_id = log_field['value']
                elif log_field['key'] == 'cnsts':
                    cnsts = [int(i) for i in log_field['value'].split("-*-") if i != '']

            if cfg_id == -1:
                parser_cfg_id = mux_id
                # print("Intra-Process Event multiplexing.")
            else:
                parser_cfg_id = cfg_id
                # print("Inter-Process Event multiplexing.")

            args_dict = {
                "pthread_id": pthread_id,
                "parent_pthread_id": parent_pthread_id,
                "parent_tid": parent_tid,
                "parent_sched_time": sched_time,
            }

            temp_dict = {
                "name": span_name, "ph": "X", "cat": "MTMC logs",
                # Here the timestemp is converted to us from ns
                "ts": start_time,
                "dur": duration,
                "tid": tid,
                "pid": 10,
                "args": args_dict
            }

            perfmon_parser.CalPmu(b_events, e_events, temp_dict["args"], cfg_idx=parser_cfg_id, cnsts=cnsts, dur_ms=float(duration)/1000)

            # # TODO: Need to check if CalTopDown flag is set
            # tmam_names, output_tmam =  perfmon_parser.CalTopDown(int(b_events[-3]), int(b_events[-2]), int(b_events[-1]),
            #                                                      int(e_events[-3]), int(e_events[-2]), int(e_events[-1]))
            # for tmam_idx, name in enumerate(tmam_names):
            #     temp_dict["args"][name] = output_tmam[tmam_idx]

            dump_dict.append(temp_dict)

    with open(output_file, 'w', encoding="utf-8") as f:
        json.dump(dump_dict, f, indent=4)

#  wget "http://127.0.0.1:16686/api/traces?service=empty-service-name&raw=true&limit=1&lookback=6h&prettyPrint=true" -O otle_tf.json
#  wget "http://127.0.0.1:16686/api/traces?service=empty-service-name&raw=true&limit=1&lookback=6h&prettyPrint=true" -O otle_mtmc_tests.json


def ParseOtleRawToTimeline(raw_path, cfg_path, output_path, is_tf=False, is_pt=False):
    file = parse_json(raw_path)
    output_file = os.path.join(output_path, 'timeline.json')
    if is_tf:
        ParseSpanToTimelineTF(file, cfg_path, output_file)
    elif is_pt:
        ParseSpanToTimelinePT(file, cfg_path, output_file)
    else :
        ParseSpanToTimeline(file, cfg_path, output_file)

def GetLogFromJaeger(jager_url, limit=1, lookback="6h"):
    ret = requests.get(os.path.join(jager_url, f"api/traces?service=empty-service-name&raw=true&limit={limit}&lookback={lookback}&prettyPrint=true"))
    if ret.status_code >= 400:
        raise RuntimeError(f"Failed to get json from jaeger [{ret.status_code}]. Please try another ip address.")
    return ret.json()

def ParseOtleFromJaeger(jager_url, cfg_path, output_path, is_tf=False, is_pt=False):
    log_json = GetLogFromJaeger(jager_url)
    output_file = os.path.join(output_path, 'timeline.json')
    if is_tf :
        ParseSpanToTimelineTF(log_json, cfg_path, output_file)
    elif is_pt :
        ParseSpanToTimelinePT(log_json, cfg_path, output_file)
    else :
        ParseSpanToTimeline(log_json, cfg_path, output_file)

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

    parser = argparse.ArgumentParser()
    parser.add_argument("-c", type=str, help="Configuration file path.", required=True)
    parser.add_argument("-o", type=str, help="Output file path.", required=True)
    parser.add_argument("-i", type=str, help="Input file path or Jaeger http address.", default="http://127.0.0.1:16686/")
    parser.add_argument("--tf", default=False, action="store_true", help="Whether the timeline comes from Tensorflow")
    parser.add_argument("--pt", default=False, action="store_true", help="Whether the timeline comes from Pytorch")

    args = parser.parse_args()

    raw_path = args.i
    config_path = args.c
    output_path = args.o
    is_tf = args.tf
    is_pt = args.pt

    path_valid = True

    path_valid &= PathCheck([args.i, args.c, args.o])
    if path_valid :
        if os.path.isfile(raw_path):
            ParseOtleRawToTimeline(raw_path, config_path, output_path, is_tf, is_pt)
        else:
            ParseOtleFromJaeger(raw_path, config_path, output_path, is_tf, is_pt)
        print("Done")
    else :
        print("Failed post processing due to invalid path")
