# C++ Example on how to use MTMC-Temp-Profiler

## Install dependency and the profiler
Please follow doc/Install.md to install all required packages and the profiler.

## Generate a configuration of use the example configuration

### Example Configuration
Instead, you could use the example configuration: example_cfg_mem.json. This configuration is target for SPR to measure Topdown metrics and memory 

### Download Perfmon infos from Intel's repo
````
git clone https://github.com/intel/perfmon
````

### Setup environment variables:

````
export MTMC_CONFIG=path to the configuration json file generated from the last step. Eg. example_cfg_mem.json
export PERFMON_GIT_PATH=absolut path to the perfmon repo
````

### Run opentele_exporter

````
opentele_exporter
````

### Make sure Jaeger Image is up

Follow https://www.jaegertracing.io/docs/1.6/getting-started/ to start Jaeger

### Compile and run cpp_example

Build and execute cpp_example.cpp using cmake.

### Run cpp_example and process log

Run cpp_example. The log will be collected by the Jaeger. You can process the log in the following ways:

- Process and visualize logs by script.
  ````
  python post_processing/otl_post_processing.py -c path_to_config_json -o path_to_export_file -i jaeger_url
  # Eg:
  python post_processing/otl_post_processing.py -c /home/user/example_cfg_mem.json -o /home/user/timeline.json -i http://127.0.0.1:16686/
  ````
  You can then visualize the timeline.json using https://ui.perfetto.dev/