# Example of profiling DLRM on Tensorflow 1.15.0

This is an example of setting up MTMC profiler on Tensorflow 1.15.0 and profile the recommendation neural network DLRM

## Getting Started

### Prerequisites

* GNU g++ >= 8.4.0
* Requirements for building Tensorflow 1.15.0 from source
* Intel 12th Gen Core / ICX Xeon for a fast Topdown telemetry collection

### Installation

#### Build Tensorflow 1.15.0 from source
In this example, we run the DLRM workload with Tensorflow 1.15.0 C++ Api. So, first, you have to be able to build the Tensorflow 1.15.0 C++ Library from source on your machine.

When building with bazel, considering using following commands:

````
$ bazel build --cxxopt=-D_GLIBCXX_USE_CXX11_ABI=0 --copt=-O3 --copt=-Wformat --copt=-Wformat-security --copt=-fstack-protector --copt=-fPIC --copt=-fpic --linkopt=-znoexecstack --linkopt=-zrelro --linkopt=-znow --linkopt=-fstack-protector --copt=-march=native //tensorflow:libtensorflow_framework.so //tensorflow:libtensorflow_cc.so
````

#### Apply MTMC profiler to the Tensorflow

Run script
````
$ apply_mtmc.sh [Path to Tensorflow source code]
````

#### Rebuild the Tensorflow

Build the tensorflow again with the same command. Then copy the shared library to the lib folder in this project.

````
$ cp [Path to Tensorflow source code]/bazel-out/k8-opt/bin/tensorflow/libtensorflow_cc.so mtmc/cpp/tests/tf/lib/libtensorflow_cc.so
$ cp [Path to Tensorflow source code]/bazel-out/k8-opt/bin/tensorflow/libtensorflow_framework.so mtmc/cpp/tests/tf/lib/libtensorflow_framework.so
````

#### Run the DLRM model with MTMC profiler

Execute script run_dlrm.sh

````
$ ./run_dlrm.sh [iterations] [timeline iterations] [inter-op threads] [intra-op threads] [mtmc config]
````

For example: Run DLRM for 100 iterations and collect timeline for 10 iterations at middle with 8 inter-op threads and 8 intra-op threads, use example Cache configs:

````
$ ./run_dlrm.sh 100 10 8 8 configExampleCaches.txt
````

#### View results

In this example, the results are stored at cpp/tests/test_logs/mtmc_tf_tests_logs/

The logs containing the following files:

* default_timeline.json: Original timeline file from the Tensorflow framework
* mtmc_raw_[thread pool id]: Raw collected logs by the profiler for a certain thread pool
* perf.data: Raw scheduling information collected by linux perf
* raw_log.json: Raw processed mtmc logs in a timeline format
* step_state: Step state file collected by the Tensorflow framework
* processed_log.json: MTMC profiler final logs that mapped application-system-uarch timeline logs

For json files, it can be view either by chrome://tracing/ or Perfetto


