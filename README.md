# Multi-Task Multi-Core Profiler

The target of this project is to provide a time domain performance profiler to analysis high concurrent deep learning workloads.

The profiler provide apis to embedded log point into your workload and an automatic analyzer to visualize the performance and generate potential optimization suggestion.

## Getting Started

We provide example codes of using the api to use the profiler, please see details in the examples directory

### Build and install the profiler

Please follow doc/Install.md.

### C++ code instrumentation example

Please follow examples/cpp_example/README.md for how to trace a C++ program 

### Example log

We provide an example processed timeline by the profiler for the DLRM workloads under examples/tensorflow/dlrm_example/example_log/.

The timeline is based on DLRM executed on Tensorflow 1.15.0. With original timeline captured by Tensorflow profiler, it expands
to profiler Intra-op threads, Core threads mappings and Per intra-op task micro-architecture telemetry.

You can view the timeline by your self with perfetto or chrome://tracing/. Simply load the .json file directly.