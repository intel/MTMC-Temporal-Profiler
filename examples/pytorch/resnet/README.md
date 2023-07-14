# Run Resnet50 with MTMC profiler on Pytorch 2.0

This is an example of runing Resnet50 with MTMC profiler on Pytorch 2.0

## Getting Started

### Prerequisites

* MTMC profiler installed
* Enabled MTMC profiler on Pytorch 2.0
* Intel 12th Gen Core / ICX Xeon for a fast Topdown telemetry collection


#### Data collection

start exporter
````
$ /usr/local/bin/opentele_exporter &
```` 

set config and run resnet50 on Pytorch:

````
$ export MTMC_CONFIG=[XXX]/cce-ai.mtmc-profiler/examples/cpp_example/example_cfg_mem.json
$ python pytorch-resnet.py
````

or on Pytorch with IPEX:
````
$ python pytorch-resnet.py --ipex
````

#### Data postprocess

get the data into json file:
````
$ wget "http://localhost:16686/api/traces?service=empty-service-name&raw=true&limit=1&lookback=6h&prettyPrint=true" -O pytorch_input.json
````

set env and postprocess the data with the script:
````
$ git clone https://github.com/intel/perfmon.git
$ export PERFMON_GIT_PATH=[XXX]/perfmon
$ python3 otl_post_processing.py -c example_cfg_mem.json --pt -i pytorch_input.json -o ./
````

and timeline.json would be generated in current dir.

#### View

For timeline.json file, it can be view either by chrome://tracing/ or Perfetto
