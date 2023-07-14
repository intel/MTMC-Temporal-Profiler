# Integrate MTMC profiler on Pytorch 2.0

This is a step-by-step of setting up MTMC profiler on Pytorch 2.0

## Getting Started

### Prerequisites

* GNU g++ >= 8.4.0
* Python >= 3.8
* Requirements for building Pytorch from source
* Intel 12th Gen Core / ICX Xeon for a fast Topdown telemetry collection

### Installation

#### Build Pytorch from source

download the source with below commands:

````
$ git clone https://github.com/pytorch/pytorch.git
$ cd pytorch
$ git checkout 10fbdcf72c82f1edcf070614a5dacf29c7a1c77c
$ git submodule update --init --recursive
````

#### Apply MTMC profiler to the Pytorch and OneDnn

run below commands:
````
$ git apply pytorch-patch.txt
$ cd third_party/ideep/mkl-dnn/ && git apply pytorch-onednn-patch.txt
````

#### Build the Pytorch

Build the Pytorch. Then copy the MTMC library to the lib folder in torch.

````
$ python setup.py install
$ cp /usr/local/lib64/libpfc.so /usr/lib64/python3.8/site-packages/torch/lib/libpfc.so 
````

#### Build Pytorch + IPEX

download the source of IPEX with below commands:

````
$ git clone https://github.com/intel/intel-extension-for-pytorch.git
$ git checkout 7d205d613130c1b344a2f3722d8894ab1bf2264d
$ git submodule update --init --recursive
````

apply MTMC profiler to the OneDnn and build:

````
$ git apply ipex-onednn-patch.txt
$ python setup.py install
````
