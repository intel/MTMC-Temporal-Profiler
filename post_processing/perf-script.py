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

from __future__ import print_function
import os
import sys
import time

sys.path.append(os.environ['PERF_EXEC_PATH'] + \
	'/scripts/python/Perf-Trace-Util/lib/Perf/Trace')

from perf_trace_context import *
from Core import *
import _pickle as pickle

test_list = []
output_core = {}
output_thread = {}
abs_path = os.path.dirname(os.path.abspath(__file__))
time_start = time.time()


def trace_begin():
	time_start = time.time()
	placeholder = 0

def trace_end():
	time_end = time.time()
	print("Time cost: %.3f s" % (time_end - time_start))

	with open(os.path.join(abs_path,"sched_info"), 'wb') as f:
		pickle.dump([output_core, output_thread], f)

def sched__sched_switch(event_name, context, common_cpu,
	common_secs, common_nsecs, common_pid, common_comm,
	common_callchain, prev_comm, prev_pid, prev_prio, prev_state, 
	next_comm, next_pid, next_prio, perf_sample_dict):

		# 0 - core, 1 - time, 2 - prev tid , 3 - prev name, 4 - next tid, 5 - next name
		data = [common_cpu, common_secs*1000*1000*1000+common_nsecs, prev_pid, prev_comm, next_pid, next_comm]

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

def trace_unhandled(event_name, context, event_fields_dict, perf_sample_dict):
	placeholder = 0

def print_header(event_name, cpu, secs, nsecs, pid, comm):
	placeholder = 0
