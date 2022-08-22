# Multi-Task Multi-Core Profiler

The target of this project is to provide a time domain performance profiler to analysis high concurrent deep learning workloads.

The profiler provide apis to embedded log point into your workload and an automatic analyzer to visualize the performance and generate potential optimization suggestion.

## Getting Started

We provide example codes of using the api to use the profiler, please see details in the examples directory

### Build and install the profiler

Use script:
```
cd scripts
./rebuild.sh
```

Or manually build with cmake

````
$ mkdir build
$ cd build
$ cmake ..
$ make -j
$ make install
````

### Example integration with tensorflow

Please follow Readme in examples folder for example of using MTMC Temporal profiler with Tensorflow 1.15.0
