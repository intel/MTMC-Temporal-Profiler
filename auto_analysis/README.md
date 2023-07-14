# Multi-Task Multi-Core Profiler -- auto analysis

Multi-Task Multi-Core Profiler provide auto analysis function for end user.

 ## Getting Started

 1. get the output json files from post processing step, for example, the output files:

	 - [ ] runtime-timeline.json : the timeline gennerated by post processing for your running.
	 - [ ] groundtruth-timeline.json : the timeline gennerated by post processing for groundtruth.

 2.  for latency target and compared with groudtruth :
	 ````
	 # mkdir output_latency_with_gt_dir
	 # python ./auto_analysis.py -g groundtruth-timeline.json  -i runtime-timeline.json -t Latency -o output_latency_with_gt_dir
	 ````
	 the output file would be stored in dir '***output_latency_with_gt_dir***'.

 3.  for latency target without comparation :
	 ````
	 # mkdir output_latency_without_gt_dir
	 # python ./auto_analysis.py -i runtime-timeline.json -t Latency -o output_latency_without_gt_dir
	 ````
		the output file would be stored in dir '***output_latency_without_gt_dir***'.


4. for QPS target and compared with groudtruth :
	````
		# mkdir output_qps_with_gt_dir
		# python ./auto_analysis.py -g groundtruth-timeline.json  -i runtime-timeline.json -t QPS -o output_qps_with_gt_dir 

	````
	the output file would be stored in dir '***output_latency_with_gt_dir***'.

 5. or for QPS target without comparation :
	 ````
	 # mkdir output_qps_without_gt_dir
	 # python ./auto_analysis.py -i runtime-timeline.json -t QPS -o output_qps_without_gt_dir
	````
	the output file would be stored in dir '***output_latency_without_gt_dir***'.
